#include "planejamento_rota.h"
#include "buffer.h"
#include "variaveis.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <cstring>
#include <string>
#include <algorithm>

#include <MQTTClient.h>

using namespace std;

// ID desta tarefa 
#define ID_TAREFA_PLAN  3

#define BROKER_ADDR     "tcp://localhost:1883"
#define CLIENT_ID       "Planejamento_CPP"
#define TOPIC_SUB       "atr/caminhao/+/setpoint" // Recebe comandos do Python
#define TOPIC_PUB       "atr/caminhao/0/estado"   // Envia estado para o Python


// Armazenam o alvo atual recebido do Python
float alvo_x = 0.0f;
float alvo_y = 0.0f;
float alvo_vel = 0.0f;
bool  tem_novo_alvo = false;

// Mutex para proteger os dados que vêm do callback MQTT
std::mutex mtx_alvo;

// Normaliza ângulo para -180 a 180
float normalizar_angulo(float ang) {
    ang = fmod(ang + 180.0f, 360.0f);
    if (ang < 0) ang += 360.0f;
    return ang - 180.0f;
}

// JSON recebido do Python: {"x": 100, "y": 200, "vel": 50}
void processar_json_setpoint(string payload) {
    try {
        size_t px = payload.find("\"x\":");
        size_t py = payload.find("\"y\":");
        size_t pv = payload.find("\"vel\":");

        if (px != string::npos && py != string::npos) {
            float nx = stof(payload.substr(px + 4));
            float ny = stof(payload.substr(py + 4));
            float nv = (pv != string::npos) ? stof(payload.substr(pv + 6)) : 40.0f;

            // Atualiza variáveis globais protegidas
            lock_guard<mutex> lock(mtx_alvo);
            alvo_x = nx;
            alvo_y = ny;
            alvo_vel = nv;
            tem_novo_alvo = true;

            cout << "[PLAN] Novo Alvo Recebido: (" << alvo_x << ", " << alvo_y << ")" << endl;
        }
    } catch (...) {
        cout << "[PLAN] Erro ao ler JSON do MQTT." << endl;
    }
}

// CALLBACKS MQTT
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    string payload((char*)message->payload, message->payloadlen);
    processar_json_setpoint(payload);
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause) {
    cout << "[PLAN] Conexão MQTT perdida!" << endl;
}

void tarefa_planejamento_rota(Buffer_Circular* buffer, std::atomic<bool>& running) {
    cout << "[Planejamento] Tarefa iniciada. Conectando ao MQTT..." << endl;

    // 1. Configuração do MQTT
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, BROKER_ADDR, CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        cout << "[PLAN] Falha ao conectar no Broker. Código: " << rc << endl;
        // Continua rodando localmente mesmo sem MQTT, para não travar tudo
    } else {
        cout << "[PLAN] Conectado ao Broker. Assinando topico..." << endl;
        MQTTClient_subscribe(client, TOPIC_SUB, 0);
    }

    // 2. Loop de Controle (Frequência ~10Hz)
    while (running) {
        // --- ETAPA A: CONSUMIDOR (Lê estado atual do Buffer) ---
        float pos_x = buffer->consumidor_i(ID_I_POS_X, ID_TAREFA_PLAN);
        float pos_y = buffer->consumidor_i(ID_I_POS_Y, ID_TAREFA_PLAN);
        float ang_atual = buffer->consumidor_i(ID_I_ANG_X, ID_TAREFA_PLAN);

        // --- ETAPA B: LÓGICA DE NAVEGAÇÃO ---
        float sp_vel_calc = 0.0f;
        float sp_ang_calc = 0.0f;

        // Copia segura do alvo
        float tx, ty, tvel;
        {
            lock_guard<mutex> lock(mtx_alvo);
            tx = alvo_x;
            ty = alvo_y;
            tvel = alvo_vel;
        }

        // Distância até o alvo
        float dx = tx - pos_x;
        float dy = ty - pos_y;
        float dist = sqrt(dx*dx + dy*dy);

        if (dist > 2.0f) { // Se estiver longe (> 2 metros), anda
            // Calcula ângulo desejado (arco tangente)
            float ang_desejado = atan2(dy, dx) * (180.0f / M_PI); // rad -> graus
            
            // O Setpoint de ângulo é o ângulo absoluto para onde o caminhão deve olhar
            sp_ang_calc = ang_desejado; 
            sp_vel_calc = tvel;
        } else {
            // Chegou no alvo, para
            sp_vel_calc = 0.0f;
            sp_ang_calc = ang_atual; // Mantém ângulo atual
        }

        // --- ETAPA C: PRODUTOR (Escreve Setpoints no Buffer) ---
        buffer->produtor_i(sp_vel_calc, ID_SP_VEL);
        buffer->produtor_i(sp_ang_calc, ID_SP_ANG);

        // --- ETAPA D: PUBLICAR ESTADO NO MQTT (Para o Python ver) ---
        // Formata JSON manualmente
        char json_msg[256];
        sprintf(json_msg, 
            "{\"id\":0, \"x\":%.2f, \"y\":%.2f, \"vel\":%.2f, \"ang\":%.2f, \"automatico\":true, \"defeito\":false}", 
            pos_x, pos_y, sp_vel_calc, ang_atual);

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = json_msg;
        pubmsg.payloadlen = strlen(json_msg);
        pubmsg.qos = 0;
        pubmsg.retained = 0;
        MQTTClient_publishMessage(client, TOPIC_PUB, &pubmsg, NULL);

        // Controle de Frequência (sleep necessário para loop de controle)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Limpeza
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    cout << "[Planejamento] Tarefa encerrada." << endl;
}