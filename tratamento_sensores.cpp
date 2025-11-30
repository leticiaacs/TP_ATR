#include "tratamento_sensores.h"
#include "buffer.h"

// ===================== MÉTODOS DA CLASSE ===================== //

void Tratamento_Sensores::tratamento_sensores(Buffer_Circular& buffer,
                                              int i_posicao_x,
                                              int i_posicao_y,
                                              int i_angulo_x)
{
    // Cada chamada trata UMA amostra bruta de cada sensor.
    // A periodicidade agora é controlada por tarefa_tratamento_sensores.

    // Threads para cada sensor (podem ser mantidas separadas, cada uma faz o seu filtro)
    thread t_x (&Tratamento_Sensores::thread_sensor_px, this,
                std::ref(buffer), i_posicao_x);
    thread t_y (&Tratamento_Sensores::thread_sensor_py, this,
                std::ref(buffer), i_posicao_y);
    thread t_ang(&Tratamento_Sensores::thread_sensor_ax, this,
                 std::ref(buffer), i_angulo_x);

    // Espera as três threads terminarem esse ciclo de processamento
    t_x.join();
    t_y.join();
    t_ang.join();
}

void Tratamento_Sensores::thread_sensor_px(Buffer_Circular& buffer, int i_posicao_x)
{
    // Recebe nova amostra bruta de posição X
    this->constroi_vetor(v_i_posicao_x, i_posicao_x);
    if (v_i_posicao_x.size() == M) {
        soma_i_posicao_x = this->filtro(v_i_posicao_x);
        this->adiciona_buffer(buffer, soma_i_posicao_x, ID_I_POS_X);
    }
}

void Tratamento_Sensores::thread_sensor_py(Buffer_Circular& buffer, int i_posicao_y)
{
    // Recebe nova amostra bruta de posição Y
    this->constroi_vetor(v_i_posicao_y, i_posicao_y);
    if (v_i_posicao_y.size() == M) {
        soma_i_posicao_y = this->filtro(v_i_posicao_y);
        this->adiciona_buffer(buffer, soma_i_posicao_y, ID_I_POS_Y);
    }
}

void Tratamento_Sensores::thread_sensor_ax(Buffer_Circular& buffer, int i_angulo_x)
{
    // Recebe nova amostra bruta do ângulo
    this->constroi_vetor(v_i_angulo_x, i_angulo_x);
    if (v_i_angulo_x.size() == M) {
        soma_i_angulo_x = this->filtro(v_i_angulo_x);
        this->adiciona_buffer(buffer, soma_i_angulo_x, ID_I_ANG_X);
    }
}

void Tratamento_Sensores::constroi_vetor(vector<int>& v, int var)
{
    if (v.size() == M) {
        v.erase(v.begin());
    }
    v.push_back(var);

    // (opcional) debug:
    std::cout << "[Tratamento Sensores] Vetor: ";
    for (size_t j = 0; j < v.size(); ++j) {
        std::cout << v[j] << " ";
    }
    std::cout << "\n";
}

float Tratamento_Sensores::filtro(vector<int>& v)
{
    float soma = 0.0f;
    for (int i = 0; i < (int)v.size(); i++) {
        soma += v[i];
    }
    float resultado = soma / v.size();
    return resultado;
}

void Tratamento_Sensores::adiciona_buffer(Buffer_Circular& buffer,
                                          float resultado,
                                          int id_sensor)
{
    buffer.produtor_i(resultado, id_sensor);
}

#include "tratamento_sensores.h"
#include "buffer.h"

#include <string>
#include <cmath>
#include <chrono>

extern "C" {
    #include <mosquitto.h>
}

// ---------------------------------------------------------------------
// Helpers para MQTT (últimos valores recebidos de sim_mina)
// ---------------------------------------------------------------------

struct TS_MQTT_Data {
    std::atomic<float> pos_x{0.0f};
    std::atomic<float> pos_y{0.0f};
    std::atomic<float> ang_x{0.0f};
};

// parser simples para pegar floats do JSON {"id":..,"pos_x":..,"pos_y":..,"ang_x":..}
static void parse_float_field(const std::string& payload,
                              const std::string& key,
                              float& out)
{
    const std::string k = "\"" + key + "\"";
    auto key_pos = payload.find(k);
    if (key_pos == std::string::npos) return;

    auto colon = payload.find(':', key_pos);
    if (colon == std::string::npos) return;

    auto end = payload.find_first_of(",}", colon + 1);
    if (end == std::string::npos) end = payload.size();

    std::string num = payload.substr(colon + 1, end - colon - 1);

    try {
        out = std::stof(num);
    } catch (...) {
        // ignora erro de conversão
    }
}

static void ts_mqtt_on_message(struct mosquitto* /*mosq*/,
                               void* userdata,
                               const struct mosquitto_message* msg)
{
    auto* data = static_cast<TS_MQTT_Data*>(userdata);
    if (!data) return;

    std::string topic  = msg->topic ? msg->topic : "";
    std::string payload(static_cast<char*>(msg->payload),
                        static_cast<size_t>(msg->payloadlen));

    // esperamos "atr/caminhao/<id>/sensores"
    if (topic.find("/sensores") == std::string::npos) {
        return;
    }

    float px = 0.0f;
    float py = 0.0f;
    float ax = 0.0f;

    parse_float_field(payload, "pos_x", px);
    parse_float_field(payload, "pos_y", py);
    parse_float_field(payload, "ang_x", ax);

    data->pos_x.store(px);
    data->pos_y.store(py);
    data->ang_x.store(ax);
}

// ---------------------------------------------------------------------
// FUNÇÃO QUE VOCÊ PEDIU (agora com MQTT embutido)
// ---------------------------------------------------------------------

void tarefa_tratamento_sensores(Buffer_Circular* buffer, 
                                std::atomic<bool>& running,
                                int caminhao_id)
{
    std::cout << "[Tratamento Sensores] Tarefa iniciada (periódica + MQTT)." << std::endl;

    Tratamento_Sensores ts;

    int i_posicao_x = 0;
    int i_posicao_y = 0;
    int i_angulo_x  = 0;

    // ---------------- MQTT: conecta e assina sensores ----------------
    TS_MQTT_Data mqtt_data;

    // const int CAMINHAO_ID = 1;  // <-- REMOVER
    std::string client_id = "trat_sensores_" + std::to_string(caminhao_id);
    std::string topic     = "atr/caminhao/" + std::to_string(caminhao_id) + "/sensores";

    mosquitto_lib_init();

    struct mosquitto* m = mosquitto_new(client_id.c_str(), true, &mqtt_data);
    if (!m) {
        std::cerr << "[Tratamento Sensores] falha mosquitto_new" << std::endl;
        mosquitto_lib_cleanup();
        return;
    }

    mosquitto_message_callback_set(m, ts_mqtt_on_message);

    int rc = mosquitto_connect(m, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[Tratamento Sensores] falha mosquitto_connect: " << rc << std::endl;
        mosquitto_destroy(m);
        mosquitto_lib_cleanup();
        return;
    }

    rc = mosquitto_subscribe(m, nullptr, topic.c_str(), 0);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[Tratamento Sensores] falha mosquitto_subscribe: " << rc << std::endl;
        mosquitto_disconnect(m);
        mosquitto_destroy(m);
        mosquitto_lib_cleanup();
        return;
    }

    // Thread para rodar o loop MQTT em paralelo com o timer do Boost.Asio
    std::thread mqtt_thread([&]() {
        while (running.load()) {
            int rc_loop = mosquitto_loop(m, 100, 1); // timeout 100 ms
            if (rc_loop != MOSQ_ERR_SUCCESS) {
                std::cerr << "[Tratamento Sensores] erro mosquitto_loop: " << rc_loop << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                mosquitto_reconnect(m);
            }
        }
    });

    // ---------------- Timer periódico (mantido do seu código) ----------------

    const int PERIODO_MS = 100;  // 10 Hz, por exemplo

    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(PERIODO_MS));

    std::function<void(const boost::system::error_code&)> handler;

    handler = [&](const boost::system::error_code& ec)
    {
        if (ec) {
            return; // erro no timer, encerra
        }

        if (!running) {
            return; // flag para encerrar tarefa
        }

        // *** AGORA: lê os últimos valores recebidos via MQTT ***
        float px = mqtt_data.pos_x.load();
        float py = mqtt_data.pos_y.load();
        float ax = mqtt_data.ang_x.load();

        i_posicao_x = static_cast<int>(std::round(px));
        i_posicao_y = static_cast<int>(std::round(py));
        i_angulo_x  = static_cast<int>(std::round(ax));

        // Chama o tratamento (filtro + gravação no buffer já existente)
        ts.tratamento_sensores(*buffer,
                               i_posicao_x,
                               i_posicao_y,
                               i_angulo_x);

        // Reagenda o próximo disparo
        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(PERIODO_MS));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();

    // ---------------- Encerramento ----------------
    std::cout << "[Tratamento Sensores] Tarefa encerrando..." << std::endl;

    running.store(false);
    mqtt_thread.join();

    mosquitto_disconnect(m);
    mosquitto_destroy(m);
    mosquitto_lib_cleanup();

    std::cout << "[Tratamento Sensores] Tarefa encerrada." << std::endl;
}
