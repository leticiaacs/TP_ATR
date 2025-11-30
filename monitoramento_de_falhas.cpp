#include "monitoramento_de_falhas.h"
#include "variaveis.h"

#include <iostream>
#include <functional>
#include <boost/asio.hpp>

extern "C" {
    #include <mosquitto.h>
}

using namespace std;

// -----------------------------------------------------------------------------
// Definições das variáveis globais declaradas em variaveis.h
// -----------------------------------------------------------------------------
mutex mtx_falha_eletrica;
mutex mtx_falha_hidraulica;
mutex mtx_falha_temp_alerta;
mutex mtx_falha_temp_defeito;
mutex mtx_defeito;
mutex mtx_rearme;

bool defeito_var_global         = false;
bool rearme_var_global          = false;
bool falha_eletrica_global      = false;
bool falha_hidraulica_global    = false;
bool falha_temp_alerta_global   = false;
bool falha_temp_defeito_global  = false;

// Sensores brutos (agora alimentados via MQTT do sim_mina)
std::atomic<int>  i_temperatura_sensor{0};
std::atomic<bool> i_falha_eletrica_sensor{false};
std::atomic<bool> i_falha_hidraulica_sensor{false};

// -----------------------------------------------------------------------------
// Helpers de parsing simples de JSON (sem biblioteca externa)
// -----------------------------------------------------------------------------
static void parse_bool_field(const std::string& payload,
                             const std::string& key,
                             bool& out)
{
    auto key_pos = payload.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return;

    auto colon = payload.find(':', key_pos);
    if (colon == std::string::npos) return;

    auto end = payload.find_first_of(",}", colon + 1);
    if (end == std::string::npos) end = payload.size();

    std::string val = payload.substr(colon + 1, end - colon - 1);

    if (val.find("true") != std::string::npos ||
        val.find("1")    != std::string::npos)
    {
        out = true;
    } else {
        out = false;
    }
}

static void parse_int_field(const std::string& payload,
                            const std::string& key,
                            int& out)
{
    auto key_pos = payload.find("\"" + key + "\"");
    if (key_pos == std::string::npos) return;

    auto colon = payload.find(':', key_pos);
    if (colon == std::string::npos) return;

    auto end = payload.find_first_of(",}", colon + 1);
    if (end == std::string::npos) end = payload.size();

    std::string val = payload.substr(colon + 1, end - colon - 1);

    try {
        out = std::stoi(val);
    } catch (...) {
        // ignora erro de conversão
    }
}

// -----------------------------------------------------------------------------
// Callback MQTT: lê o tópico de falhas do sim_mina
// -----------------------------------------------------------------------------
struct FalhasMQTTContext {
    int caminhao_id;
};

static void mqtt_on_message(struct mosquitto*,
                            void* userdata,
                            const struct mosquitto_message* msg)
{
    auto* ctx = static_cast<FalhasMQTTContext*>(userdata);
    if (!ctx) return;

    std::string topic = msg->topic ? msg->topic : "";
    std::string payload(static_cast<char*>(msg->payload),
                        static_cast<size_t>(msg->payloadlen));

    // Tópico esperado: atr/caminhao/<id>/falhas
    std::string expected_topic = "atr/caminhao/" + std::to_string(ctx->caminhao_id) + "/falhas";
    if (topic != expected_topic) {
        return;
    }

    int  temp  = 0;
    bool fe    = false;
    bool fh    = false;

    parse_int_field(payload,  "temp",            temp);
    parse_bool_field(payload, "falha_eletrica",  fe);
    parse_bool_field(payload, "falha_hidraulica", fh);

    i_temperatura_sensor.store(temp);
    i_falha_eletrica_sensor.store(fe);
    i_falha_hidraulica_sensor.store(fh);
}

// -----------------------------------------------------------------------------
// Tarefa principal de monitoramento de falhas
// -----------------------------------------------------------------------------
void tarefa_monitoramento_falhas(Buffer_Circular* /*buffer*/,
                                 std::atomic<bool>& running,
                                 int caminhao_id,
                                 const std::string& broker,
                                 int broker_port)
{
    cout << "[Monitoramento Falhas] Tarefa iniciada." << endl;

    // Inicializa estados globais
    {
        lock_guard<mutex> lk(mtx_defeito);
        defeito_var_global = false;
    }
    {
        lock_guard<mutex> lk(mtx_falha_eletrica);
        falha_eletrica_global = false;
    }
    {
        lock_guard<mutex> lk(mtx_falha_hidraulica);
        falha_hidraulica_global = false;
    }
    {
        lock_guard<mutex> lk(mtx_falha_temp_alerta);
        falha_temp_alerta_global = false;
    }
    {
        lock_guard<mutex> lk(mtx_falha_temp_defeito);
        falha_temp_defeito_global = false;
    }
    {
        lock_guard<mutex> lk(mtx_rearme);
        rearme_var_global = false;
    }

    // -----------------------------------------------------------------
    // MQTT: assina atr/caminhao/<id>/falhas do sim_mina.py
    // -----------------------------------------------------------------
    mosquitto_lib_init();

    FalhasMQTTContext ctx;
    ctx.caminhao_id = caminhao_id;

    std::string client_id = "monit_falhas_" + std::to_string(caminhao_id);
    struct mosquitto* m = mosquitto_new(client_id.c_str(), true, &ctx);
    if (!m) {
        std::cerr << "[Monitoramento Falhas] Erro: mosquitto_new" << std::endl;
        mosquitto_lib_cleanup();
        return;
    }

    mosquitto_message_callback_set(m, mqtt_on_message);

    int rc = mosquitto_connect(m, broker.c_str(), broker_port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[Monitoramento Falhas] Erro mosquitto_connect: " << rc << std::endl;
        mosquitto_destroy(m);
        mosquitto_lib_cleanup();
        return;
    }

    std::string topic = "atr/caminhao/" + std::to_string(caminhao_id) + "/falhas";
    rc = mosquitto_subscribe(m, nullptr, topic.c_str(), 0);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[Monitoramento Falhas] Erro mosquitto_subscribe: " << rc << std::endl;
        mosquitto_disconnect(m);
        mosquitto_destroy(m);
        mosquitto_lib_cleanup();
        return;
    }

    // Thread para manter o loop MQTT vivo enquanto o ASIO roda
    std::thread mqtt_thread([&]() {
        while (running.load()) {
            int rc_loop = mosquitto_loop(m, 100, 1); // timeout 100ms
            if (rc_loop != MOSQ_ERR_SUCCESS) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                mosquitto_reconnect(m);
            }
        }
    });

    // -----------------------------------------------------------------
    // Lógica original dos timers (usa i_temperatura_sensor, i_falha_*_sensor)
    // -----------------------------------------------------------------
    boost::asio::io_context io;

    using Timer = boost::asio::steady_timer;
    auto t_eletrica   = std::make_shared<Timer>(io, boost::asio::chrono::milliseconds(200));
    auto t_hidraulica = std::make_shared<Timer>(io, boost::asio::chrono::milliseconds(200));
    auto t_temp       = std::make_shared<Timer>(io, boost::asio::chrono::milliseconds(200));
    auto t_rearme     = std::make_shared<Timer>(io, boost::asio::chrono::milliseconds(100));

    std::function<void(const boost::system::error_code&)> h_eletrica;
    std::function<void(const boost::system::error_code&)> h_hidraulica;
    std::function<void(const boost::system::error_code&)> h_temp;
    std::function<void(const boost::system::error_code&)> h_rearme;

    // ------------------------- Falha elétrica -------------------------
    h_eletrica = [&, t_eletrica](const boost::system::error_code& ec) {
        if (ec || !running.load()) return;

        bool falha = i_falha_eletrica_sensor.load();

        {
            lock_guard<mutex> lk(mtx_falha_eletrica);
            falha_eletrica_global = falha;
        }

        if (falha) {
            cout << "[Monitoramento Falhas] Falha elétrica detectada!" << endl;
            lock_guard<mutex> lk(mtx_defeito);
            defeito_var_global = true;
        }

        t_eletrica->expires_at(t_eletrica->expiry() + boost::asio::chrono::milliseconds(200));
        t_eletrica->async_wait(h_eletrica);
    };

    // ------------------------- Falha hidráulica -------------------------
    h_hidraulica = [&, t_hidraulica](const boost::system::error_code& ec) {
        if (ec || !running.load()) return;

        bool falha = i_falha_hidraulica_sensor.load();

        {
            lock_guard<mutex> lk(mtx_falha_hidraulica);
            falha_hidraulica_global = falha;
        }

        if (falha) {
            cout << "[Monitoramento Falhas] Falha hidráulica detectada!" << endl;
            lock_guard<mutex> lk(mtx_defeito);
            defeito_var_global = true;
        }

        t_hidraulica->expires_at(t_hidraulica->expiry() + boost::asio::chrono::milliseconds(200));
        t_hidraulica->async_wait(h_hidraulica);
    };

    // ------------------------- Temperatura -------------------------
    h_temp = [&, t_temp](const boost::system::error_code& ec) {
        if (ec || !running.load()) return;

        int temperatura = i_temperatura_sensor.load();

        bool critico = (temperatura > 120);
        bool alerta  = (temperatura > 95 && temperatura <= 120);

        if (critico) {
            cout << "[Monitoramento Falhas] ALERTA CRÍTICO: Temp > 120°C" << endl;
        } else if (alerta) {
            cout << "[Monitoramento Falhas] Alerta: Temp > 95°C" << endl;
        }

        {
            lock_guard<mutex> lk(mtx_falha_temp_defeito);
            falha_temp_defeito_global = critico;
        }
        {
            lock_guard<mutex> lk(mtx_falha_temp_alerta);
            falha_temp_alerta_global = alerta;
        }

        if (critico) {
            lock_guard<mutex> lk(mtx_defeito);
            defeito_var_global = true;
        }

        t_temp->expires_at(t_temp->expiry() + boost::asio::chrono::milliseconds(200));
        t_temp->async_wait(h_temp);
    };

    // ------------------------- Rearme -------------------------
    h_rearme = [&, t_rearme](const boost::system::error_code& ec) {
        if (ec || !running.load()) return;

        bool pede_rearme = false;
        {
            lock_guard<mutex> lk(mtx_rearme);
            pede_rearme = rearme_var_global;
        }

        bool tem_eletrica, tem_hidraulica, tem_temp_def;
        {
            lock_guard<mutex> lk(mtx_falha_eletrica);
            tem_eletrica = falha_eletrica_global;
        }
        {
            lock_guard<mutex> lk(mtx_falha_hidraulica);
            tem_hidraulica = falha_hidraulica_global;
        }
        {
            lock_guard<mutex> lk(mtx_falha_temp_defeito);
            tem_temp_def = falha_temp_defeito_global;
        }

        if (pede_rearme && !tem_eletrica && !tem_hidraulica && !tem_temp_def) {
            lock_guard<mutex> lk(mtx_defeito);
            defeito_var_global = false;
            cout << "[Monitoramento Falhas] Rearme realizado." << endl;
        }

        t_rearme->expires_at(t_rearme->expiry() + boost::asio::chrono::milliseconds(100));
        t_rearme->async_wait(h_rearme);
    };

    // Inicia os timers
    t_eletrica->async_wait(h_eletrica);
    t_hidraulica->async_wait(h_hidraulica);
    t_temp->async_wait(h_temp);
    t_rearme->async_wait(h_rearme);

    // Roda o loop de eventos
    io.run();

    // Encerramento
    running.store(false);
    mqtt_thread.join();
    mosquitto_disconnect(m);
    mosquitto_destroy(m);
    mosquitto_lib_cleanup();

    cout << "[Monitoramento Falhas] Tarefa encerrada." << endl;
}
