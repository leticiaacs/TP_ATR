#ifndef TRATAMENTO_SENSORES_H
#define TRATAMENTO_SENSORES_H

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <string>
#include <cmath>
#include <chrono>
extern "C" {
    #include <mosquitto.h>
}
#include <boost/asio.hpp>  // ainda pode ser usado em outras partes, não atrapalha
#include "buffer.h"
#include "variaveis.h"

using namespace std;

#define M 5

class Tratamento_Sensores {

private:
    vector<int> v_i_posicao_x = {0, 0, 0, 0, 0};
    vector<int> v_i_posicao_y = {0, 0, 0, 0, 0};
    vector<int> v_i_angulo_x  = {0, 0, 0, 0, 0};

    float soma_i_posicao_x = 0;
    float soma_i_posicao_y = 0;
    float soma_i_angulo_x  = 0;
    
    void thread_sensor_px(Buffer_Circular& buffer, int i_posicao_x);
    void thread_sensor_py(Buffer_Circular& buffer, int i_posicao_y);
    void thread_sensor_ax(Buffer_Circular& buffer, int i_angulo_x);

    void constroi_vetor(vector<int>& v, int var);
    float filtro(vector<int>& v);
    void adiciona_buffer(Buffer_Circular& buffer, float resultado, int id_sensor);

public:
    void tratamento_sensores(Buffer_Circular& buffer,
                             int i_posicao_x,
                             int i_posicao_y,
                             int i_angulo_x);
};

/**
 * @brief TAREFA de tratamento de sensores.
 * - Assina via MQTT o tópico atr/caminhao/<id>/sensores (sim_mina).
 * - A cada mensagem recebida, filtra (média móvel) e grava no buffer.
 */
void tarefa_tratamento_sensores(Buffer_Circular* buffer, 
                                std::atomic<bool>& running,
                                int caminhao_id);

#endif
