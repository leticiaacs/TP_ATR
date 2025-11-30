#include "tratamento_sensores.h"
#include "buffer.h"

// ===================== MÉTODOS DA CLASSE ===================== //

void Tratamento_Sensores::tratamento_sensores(Buffer_Circular& buffer,
                                              float i_posicao_x,
                                              float i_posicao_y,
                                              float i_angulo_x)
{
    // Cada chamada trata UMA amostra bruta de cada sensor.
    // A periodicidade será controlada externamente (tarefa_tratamento_sensores).

    std::thread t_x(&Tratamento_Sensores::thread_sensor_px, this,
                    std::ref(buffer), i_posicao_x);
    std::thread t_y(&Tratamento_Sensores::thread_sensor_py, this,
                    std::ref(buffer), i_posicao_y);
    std::thread t_ang(&Tratamento_Sensores::thread_sensor_ax, this,
                      std::ref(buffer), i_angulo_x);

    t_x.join();
    t_y.join();
    t_ang.join();
}

void Tratamento_Sensores::thread_sensor_px(Buffer_Circular& buffer, float i_posicao_x)
{
    this->constroi_vetor(v_i_posicao_x, i_posicao_x);
    if (v_i_posicao_x.size() == M) {
        soma_i_posicao_x = this->filtro(v_i_posicao_x);
        this->adiciona_buffer(buffer, soma_i_posicao_x, ID_I_POS_X);
    }
}

void Tratamento_Sensores::thread_sensor_py(Buffer_Circular& buffer, float i_posicao_y)
{
    this->constroi_vetor(v_i_posicao_y, i_posicao_y);
    if (v_i_posicao_y.size() == M) {
        soma_i_posicao_y = this->filtro(v_i_posicao_y);
        this->adiciona_buffer(buffer, soma_i_posicao_y, ID_I_POS_Y);
    }
}

void Tratamento_Sensores::thread_sensor_ax(Buffer_Circular& buffer, float i_angulo_x)
{
    this->constroi_vetor(v_i_angulo_x, i_angulo_x);
    if (v_i_angulo_x.size() == M) {
        soma_i_angulo_x = this->filtro(v_i_angulo_x);
        this->adiciona_buffer(buffer, soma_i_angulo_x, ID_I_ANG_X);
    }
}

void Tratamento_Sensores::constroi_vetor(std::vector<float>& v, float var)
{
    if (v.size() == M) {
        v.erase(v.begin());
    }
    v.push_back(var);

    // DEBUG: ver construção do vetor
    std::cout << "[TS] constroi_vetor: novo=" << var 
              << " size=" << v.size() << " => ";
    for (auto x : v) std::cout << x << " ";
    std::cout << std::endl;
}

float Tratamento_Sensores::filtro(const std::vector<float>& v)
{
    float soma = 0.0f;
    for (float x : v) {
        soma += x;
    }
    float resultado = soma / static_cast<float>(v.size());

    // DEBUG: ver cálculo da média
    std::cout << "[TS] filtro: ";
    for (auto x : v) std::cout << x << " ";
    std::cout << " => média=" << resultado << std::endl;

    return resultado;
}

void Tratamento_Sensores::adiciona_buffer(Buffer_Circular& buffer,
                                          float resultado,
                                          int id_sensor)
{
    std::cout << "[TS] adiciona_buffer: id=" << id_sensor 
              << " valor=" << resultado << std::endl;
    buffer.produtor_i(resultado, id_sensor);
}

// =====================================================================
//  PARTE MQTT + TAREFA PERIÓDICA (VERSÃO SIMPLIFICADA)
// =====================================================================

#include <string>
#include <thread>
#include <chrono>

extern "C" {
    #include <mosquitto.h>
}

// ---------------------------------------------------------------------
// Estrutura com os últimos dados recebidos via MQTT
// ---------------------------------------------------------------------

struct TS_MQTT_Data {
    std::atomic<float> pos_x{0.0f};
    std::atomic<float> pos_y{0.0f};
    std::atomic<float> ang_x{0.0f};
    std::atomic<bool>  has_sample{false};
};

// Parser simples de JSON (campos numéricos)
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

// Callback MQTT
static void ts_mqtt_on_message(struct mosquitto* /*mosq*/,
                               void* userdata,
                               const struct mosquitto_message* msg)
{
    auto* data = static_cast<TS_MQTT_Data*>(userdata);
    if (!data) return;

    std::string topic  = msg->topic ? msg->topic : "";
    std::string payload(static_cast<char*>(msg->payload),
                        static_cast<size_t>(msg->payloadlen));

    if (topic.find("/sensores") == std::string::npos) {
        return;
    }

    float px = 0.0f;
    float py = 0.0f;
    float ax = 0.0f;

    parse_float_field(payload, "pos_x", px);
    parse_float_field(payload, "pos_y", py);
    parse_float_field(payload, "ang_x", ax);

    // DEBUG MQTT
    std::cout << "[TS MQTT] msg em " << topic
              << " payload=" << payload
              << " -> px=" << px << " py=" << py << " ax=" << ax
              << std::endl;

    data->pos_x.store(px);
    data->pos_y.store(py);
    data->ang_x.store(ax);
    data->has_sample.store(true);
}

// ---------------------------------------------------------------------
// Tarefa principal (periodicidade + MQTT simplificado)
// ---------------------------------------------------------------------

void tarefa_tratamento_sensores(Buffer_Circular* buffer, 
                                std::atomic<bool>& running,
                                int caminhao_id)
{
    std::cout << "[TS] Tarefa iniciada (MQTT + loop simples)." << std::endl;

    if (!buffer) {
        std::cerr << "[TS] ERRO: buffer nulo!" << std::endl;
        return;
    }

    Tratamento_Sensores ts;

    TS_MQTT_Data mqtt_data;

    std::string client_id = "trat_sensores_" + std::to_string(caminhao_id);
    std::string topic     = "atr/caminhao/" + std::to_string(caminhao_id) + "/sensores";

    mosquitto_lib_init();

    struct mosquitto* m = mosquitto_new(client_id.c_str(), true, &mqtt_data);
    if (!m) {
        std::cerr << "[TS] falha mosquitto_new" << std::endl;
        mosquitto_lib_cleanup();
        return;
    }

    mosquitto_message_callback_set(m, ts_mqtt_on_message);

    int rc = mosquitto_connect(m, "localhost", 1883, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[TS] falha mosquitto_connect: " << rc << std::endl;
        mosquitto_destroy(m);
        mosquitto_lib_cleanup();
        return;
    }

    std::cout << "[TS] MQTT conectado. Assinando tópico: " << topic << std::endl;

    rc = mosquitto_subscribe(m, nullptr, topic.c_str(), 0);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[TS] falha mosquitto_subscribe: " << rc << std::endl;
        mosquitto_disconnect(m);
        mosquitto_destroy(m);
        mosquitto_lib_cleanup();
        return;
    }

    // Inicia loop MQTT em thread interna da biblioteca
    rc = mosquitto_loop_start(m);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[TS] falha mosquitto_loop_start: " << rc << std::endl;
        mosquitto_disconnect(m);
        mosquitto_destroy(m);
        mosquitto_lib_cleanup();
        return;
    }

    std::cout << "[TS] loop MQTT iniciado. Entrando no loop de amostragem." << std::endl;

    const int PERIODO_MS = 100;  // 10 Hz

    while (running.load()) {
        // Espera o período
        std::this_thread::sleep_for(std::chrono::milliseconds(PERIODO_MS));

        // Só processa se já chegou ao menos uma amostra
        if (!mqtt_data.has_sample.load()) {
            std::cout << "[TS] aguardando primeira amostra MQTT..." << std::endl;
            continue;
        }

        // Lê últimos valores
        float px = mqtt_data.pos_x.load();
        float py = mqtt_data.pos_y.load();
        float ax = mqtt_data.ang_x.load();

        std::cout << "[TS] LOOP: px=" << px
                  << " py=" << py
                  << " ax=" << ax << std::endl;

        // Chama pipeline de tratamento (filtro + gravação em buffer)
        ts.tratamento_sensores(*buffer, px, py, ax);
    }

    std::cout << "[TS] Encerrando tarefa..." << std::endl;

    // Para o loop interno do MQTT
    mosquitto_loop_stop(m, true);
    mosquitto_disconnect(m);
    mosquitto_destroy(m);
    mosquitto_lib_cleanup();

    std::cout << "[TS] Tarefa encerrada." << std::endl;
}
