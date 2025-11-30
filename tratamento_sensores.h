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

#include "buffer.h"
#include "variaveis.h"

#define M 5

class Tratamento_Sensores {

private:
    // Vetores de amostras (float)
    std::vector<float> v_i_posicao_x;
    std::vector<float> v_i_posicao_y;
    std::vector<float> v_i_angulo_x;

    float soma_i_posicao_x = 0.0f;
    float soma_i_posicao_y = 0.0f;
    float soma_i_angulo_x  = 0.0f;
    
    void thread_sensor_px(Buffer_Circular& buffer, float i_posicao_x);
    void thread_sensor_py(Buffer_Circular& buffer, float i_posicao_y);
    void thread_sensor_ax(Buffer_Circular& buffer, float i_angulo_x);

    void constroi_vetor(std::vector<float>& v, float var);
    float filtro(const std::vector<float>& v);
    void adiciona_buffer(Buffer_Circular& buffer, float resultado, int id_sensor);

public:
    void tratamento_sensores(Buffer_Circular& buffer,
                             float i_posicao_x,
                             float i_posicao_y,
                             float i_angulo_x);
};

/**
 * @brief Tarefa de tratamento de sensores.
 * 
 * - Se conecta via MQTT à simulação da mina (sim_mina).
 * - Assina o tópico atr/caminhao/<id>/sensores.
 * - A cada 100 ms lê os últimos valores de pos_x, pos_y, ang_x,
 *   passa pelo filtro (média móvel) e grava no Buffer_Circular.
 */
void tarefa_tratamento_sensores(Buffer_Circular* buffer, 
                                std::atomic<bool>& running,
                                int caminhao_id);

#endif
