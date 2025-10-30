//IDEIA: STRUCT COM VARIOS VETORES DE DIFERENTES TIPOS, CADA UM SERÁ REFERENTE A UMA VARIÁVEL. TEREMOS 
//MAIS DE UM CONSUMIDOR PARA CADA VARIÁVEL. O TEMPO DE PERIODICIDADE PARA CADA TAREFA CONSUMIDORA DEVE ESTAR
//ALINHADO COM O TEMPO DE PERIODICIDADE PARA CADA TAREFA PRODUTORA, PARA QUE OS CONSUMIDORES NAO PEGUEM 
//INFORMACOES DESATUALIZADAS.

/*Cada variável terá um ID: 
i_posicao_x - 0
i_posicao_y - 1
i_angulo_x - 2
i_temperatura - 3
o_aceleracao - 4
o_direcao - 5
c_acelera - 6
c_direita - 7
c_esquerda - 8

i_falha_eletrica - 9
i_falha_hidraulica - 10
e_defeito - 11
e_automatico - 12
c_automatico - 13
c_man - 14
c_rearme - 15

*/

//CRIAR UM BUFFER NO MAIN

//PASSAR COMO PARÂMETRO NAS OUTRAS FUNÇÕES ID_SENSOR.

#ifndef BUFFER_H
#define BUFFER_H

#include <semaphore>
#include <mutex>
#include <vector>
#include <memory>
using namespace std;

//Número de colunas da matriz - número de posições de cada vetor
#define N_VEC 200
//Número de linhas da matriz dados_i - numero total de variáveis do tipo int (sensores, atuadores, comandos e estados)
#define N_VAR_I 9
//Número de linhas da matriz dados_b - numero total de variáveis do tipo bool (sensores, atuadores, comandos e estados)
#define N_VAR_B 7

class Buffer_Circular {
public:
    Buffer_Circular();

    void produtor_i(int i, int id_var);
    int consumidor_i(int id_var);
    void produtor_b(bool i, int id_var);
    bool consumidor_b(int id_var);

private:
    int dados_i[N_VAR_I][N_VEC] = {0};
    bool dados_b[N_VAR_B][N_VEC] = {false};

    int in_i[N_VAR_I] = {0};
    int in_b[N_VAR_B] = {0};

    int out_i[N_VAR_I] = {0};
    int out_b[N_VAR_B] = {0};

    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_vazias_i;
    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_cheias_i;
    vector<unique_ptr<mutex>> mtx_i;

    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_vazias_b;
    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_cheias_b;
    vector<unique_ptr<mutex>> mtx_b;

    void produz_i(int i, int id_var);
    int consome_i(int id_var);

    void produz_b(bool i, int id_var);
    bool consome_b(int id_var);
};

#endif