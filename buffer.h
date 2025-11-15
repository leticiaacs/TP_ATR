//IDEIA: STRUCT COM VARIOS VETORES DE DIFERENTES TIPOS, CADA UM SERÁ REFERENTE A UMA VARIÁVEL. TEREMOS 
//MAIS DE UM CONSUMIDOR PARA CADA VARIÁVEL. O TEMPO DE PERIODICIDADE PARA CADA TAREFA CONSUMIDORA DEVE ESTAR
//ALINHADO COM O TEMPO DE PERIODICIDADE PARA CADA TAREFA PRODUTORA, PARA QUE OS CONSUMIDORES NAO PEGUEM 
//INFORMACOES DESATUALIZADAS.


/*Cada variável terá um ID: 
    i_posicao_x - 0
    i_posicao_y - 1
    i_angulo_x - 2
    c_acelera - 3
    c_direita - 4
    c_esquerda - 5
    setpoint_vel - 6
    setpoint_ang_x - 7

    e_defeito - 8
    e_automatico - 9
    c_automatico - 10
    c_man - 11
    c_rearme - 12

Cada tarefa também terá um ID - serve para garantir que a mesma tarefa não consuma a mesma posição duas vezes.
    logica_comando 0
    coletor_dados 1
    controle_navegacao 2
    planejamento_rota 3

*/

//CRIAR UM BUFFER NO MAIN

//PASSAR COMO PARÂMETRO NAS OUTRAS FUNÇÕES ID_SENSOR.

#ifndef BUFFER_H
#define BUFFER_H


#include <semaphore>
#include <mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <iomanip>
#include <thread>
using namespace std;

//Número de colunas da matriz - número de posições de cada vetor
#define N_VEC 20
//Número de linhas da matriz dados_i - numero total de variáveis do tipo int (sensores, atuadores, comandos e estados)
#define N_VAR_I 8
//Número de linhas da matriz dados_b - numero total de variáveis do tipo bool (sensores, atuadores, comandos e estados)
#define N_VAR_B 5
//Número de possíveis tarefas consumidoras
#define N_CONSUMIDORES 4

struct DadoInt {
    float valor = 0;
    atomic<int> leitores_restantes{0};
};

struct DadoBool {
    bool valor = false;
    atomic<int> leitores_restantes{0};
};

class Buffer_Circular {
public:
    Buffer_Circular();
    void mostrar_buffer();

    void produtor_i(float i, int id_var);
    float consumidor_i(int id_var, int id_tarefa);
    void produtor_b(bool i, int id_var);
    bool consumidor_b(int id_var, int id_tarefa);

private:
    DadoInt dados_i[N_VAR_I][N_VEC] = {0};
    DadoBool dados_b[N_VAR_B][N_VEC] = {false};

    int in_i[N_VAR_I] = {0};
    int in_b[N_VAR_B] = {0};

    int out_i[N_VAR_I][N_CONSUMIDORES] = {0};
    int out_b[N_VAR_B][N_CONSUMIDORES] = {0};

    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_vazias_i;
    vector<vector<unique_ptr<counting_semaphore<1>>>> posicoes_cheias_i;
    vector<unique_ptr<mutex>> mtx_i;
    vector<int> num_consumidores_i;

    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_vazias_b;
    vector<vector<unique_ptr<counting_semaphore<1>>>> posicoes_cheias_b;
    vector<unique_ptr<mutex>> mtx_b;
    vector<int> num_consumidores_b;

    void produz_i(float i, int id_var);
    float consome_i(int id_var, int id_tarefa);

    void produz_b(bool i, int id_var);
    bool consome_b(int id_var, int id_tarefa);
};

#endif