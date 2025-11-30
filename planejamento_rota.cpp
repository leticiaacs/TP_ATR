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
#include <vector>
#include <memory>

#include <MQTTClient.h>

using namespace std;

#define ID_TAREFA_PLAN  3
#define NUM_CAMINHOES   200 

#define BROKER_ADDR     "tcp://localhost:1884" 

struct DadosPlanejamento {
    float alvo_x = 0.0f;
    float alvo_y = 0.0f;
    float alvo_vel = 0.0f;
    bool  tem_novo_alvo = false;
    std::mutex mtx;
};

// Vetor global para armazenar os dados de todos os caminhões
static vector<unique_ptr<DadosPlanejamento>> dados_caminhoes;
static std::once_flag flag_init_plan;

void init_dados_plan() {
    for(int i=0; i<NUM_CAMINHOES; i++) {
        dados_caminhoes.push_back(make_unique<DadosPlanejamento>());
    }
}

void processar_json_setpoint(string payload, string topic) {
    try {

        int id = -1;
        size_t pos1 = topic.find("caminhao/");
        size_t pos2 = topic.find("/setpoint");
        
        if (pos1 != string::npos && pos2 != string::npos) {
            string s_id = topic.substr(pos1 + 9, pos2 - (pos1 + 9));
            id = stoi(s_id);
        }

        if (id < 0 || id >= NUM_CAMINHOES) return; // ID inválido

        // Parse do JSON
        size_t px = payload.find("\"x\":");
        size_t py = payload.find("\"y\":");
        size_t pv = payload.find("\"vel\":");

        if (px != string::npos && py != string::npos) {
            float nx = stof(payload.substr(px + 4));
            float ny = stof(payload.substr(py + 4));
            float nv = (pv != string::npos) ? stof(payload.substr(pv + 6)) : 40.0f;

            // Atualiza o caminhão específico
            DadosPlanejamento* d = dados_caminhoes[id].get();
            {
                lock_guard<mutex> lock(d->mtx);
                d->alvo_x = nx;
                d->alvo_y = ny;
                d->alvo_vel = nv;
                d->tem_novo_alvo = true;
            }
            cout << "[PLAN " << id << "] Novo Alvo: (" << nx << ", " << ny << ")" << endl;
        }
    } catch (...) {
        cout << "[PLAN] Erro no parse JSON." << endl;
    }
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    string payload((char*)message->payload, message->payloadlen);
    string topic(topicName);
    
    processar_json_setpoint(payload, topic);
    
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause) {
    cout << "[PLAN] Conexão MQTT perdida!" << endl;
}

// TAREFA PRINCIPAL - por caminhão
void tarefa_planejamento_rota(Buffer_Circular* buffer, std::atomic<bool>& running, int id_caminhao) {
    
    call_once(flag_init_plan, init_dados_plan); // Inicializa vetor global uma vez

    cout << "[Planejamento " << id_caminhao << "] Iniciado." << endl;

    // 1. Configuração MQTT
    
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    string client_id = "Plan_CPP_" + to_string(id_caminhao);
    string topic_sub = "atr/caminhao/" + to_string(id_caminhao) + "/setpoint";
    string topic_pub = "atr/caminhao/" + to_string(id_caminhao) + "/estado";

    MQTTClient_create(&client, BROKER_ADDR, client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        cout << "[PLAN " << id_caminhao << "] Falha MQTT." << endl;
        // Continua rodando local
    } else {
        MQTTClient_subscribe(client, topic_sub.c_str(), 0);
    }

    // Ponteiro para os dados deste caminhão
    DadosPlanejamento* meu_dado = dados_caminhoes[id_caminhao].get();

    while (running) {
        // --- ETAPA A: LEITURA ---

        float pos_x = buffer->consumidor_i(ID_I_POS_X, ID_TAREFA_PLAN);
        float pos_y = buffer->consumidor_i(ID_I_POS_Y, ID_TAREFA_PLAN);
        float ang_atual = buffer->consumidor_i(ID_I_ANG_X, ID_TAREFA_PLAN);
        bool  auto_mode = buffer->consumidor_b(ID_E_AUTOMATICO, ID_TAREFA_PLAN);
        bool  defeito   = buffer->consumidor_b(ID_E_DEFEITO, ID_TAREFA_PLAN);

        // --- ETAPA B: LÓGICA ---
        float sp_vel_calc = 0.0f;
        float sp_ang_calc = 0.0f;
        float tx, ty, tvel;

        {
            lock_guard<mutex> lock(meu_dado->mtx);
            tx = meu_dado->alvo_x;
            ty = meu_dado->alvo_y;
            tvel = meu_dado->alvo_vel;
        }

        float dx = tx - pos_x;
        float dy = ty - pos_y;
        float dist = sqrt(dx*dx + dy*dy);

        if (dist > 2.0f) {
            float ang_desejado = atan2(dy, dx) * (180.0f / M_PI);
            sp_ang_calc = ang_desejado;
            sp_vel_calc = tvel;
        } else {
            sp_vel_calc = 0.0f;
            sp_ang_calc = ang_atual;
        }

        // --- ETAPA C: ESCRITA ---
        buffer->produtor_i(sp_vel_calc, ID_SP_VEL);
        buffer->produtor_i(sp_ang_calc, ID_SP_ANG_X);

        // --- ETAPA D: MQTT ---
        char json_msg[512];
        sprintf(json_msg, 
            "{\"id\":%d, \"x\":%.2f, \"y\":%.2f, \"vel\":%.2f, \"ang\":%.2f, \"automatico\":%s, \"defeito\":%s}", 
            id_caminhao, pos_x, pos_y, sp_vel_calc, ang_atual,
            auto_mode ? "true" : "false",
            defeito ? "true" : "false"
        );

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = json_msg;
        pubmsg.payloadlen = strlen(json_msg);
        pubmsg.qos = 0;
        pubmsg.retained = 0;
        MQTTClient_publishMessage(client, topic_pub.c_str(), &pubmsg, NULL);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
}