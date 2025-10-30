
#ifndef TRATAMENTO_SENSORES_H
#define TRATAMENTO_SENSORES_H

#include <iostream>
#include <vector>
#include <thread>
#include "buffer.h"
using namespace std;

#define M 5

class Tratamento_Sensores{

private:
    vector<int> v_i_posicao_x = {0};
    vector<int> v_i_posicao_y = {0};
    vector<int> v_i_angulo_x = {0};

    int soma_i_posicao_x = 0;
    int soma_i_posicao_y = 0;
    int soma_i_angulo_x = 0;
    
    void thread_sensor_px(Buffer_Circular& buffer, int i_posicao_x);

    void thread_sensor_py(Buffer_Circular& buffer, int i_posicao_y);

    void thread_sensor_ax(Buffer_Circular& buffer, int i_angulo_x);

    void constroi_vetor(vector<int>& v, int var);

    int filtro(vector<int>& v);

    void adiciona_buffer(Buffer_Circular& buffer, int resultado, int id_sensor);


public:
    void tratamento_sensores(Buffer_Circular& buffer, int i_posicao_x, int i_posicao_y, int i_angulo_x);

};

#endif