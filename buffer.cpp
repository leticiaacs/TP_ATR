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

#include "buffer.h"
#include <iostream>

Buffer_Circular::Buffer_Circular() {
    for (int i = 0; i < N_VAR_I; ++i) {
        posicoes_vazias_i.push_back(make_unique<counting_semaphore<N_VEC>>(N_VEC));
        posicoes_cheias_i.push_back(make_unique<counting_semaphore<N_VEC>>(0));
        mtx_i.push_back(make_unique<mutex>());
    }
    for (int i = 0; i < N_VAR_B; ++i) {
        posicoes_vazias_b.push_back(make_unique<counting_semaphore<N_VEC>>(N_VEC));
        posicoes_cheias_b.push_back(make_unique<counting_semaphore<N_VEC>>(0));
        mtx_b.push_back(make_unique<mutex>());
    }
}

void Buffer_Circular::produz_i(int i, int id_var) {
    this->dados_i[id_var][this->in_i[id_var]] = i;
    this->in_i[id_var] = (this->in_i[id_var] + 1) % N_VEC;
}

int Buffer_Circular::consome_i(int id_var) {
    int item;
    item = this->dados_i[id_var][this->out_i[id_var]];
    this->out_i[id_var] = (this->out_i[id_var] + 1) % N_VEC;
    return item;
}

void Buffer_Circular::produtor_i(int i, int id_var) {
    posicoes_vazias_i[id_var]->acquire(); 
    mtx_i[id_var]->lock();
    produz_i(i, id_var);
    mtx_i[id_var]->unlock();
    std::cout << "Producer produced " << i << std::endl;
    posicoes_cheias_i[id_var]->release();  
}

bool Buffer_Circular::consumidor_i(int id_var) {
    int item;
    posicoes_cheias_i[id_var]->acquire();
    mtx_i[id_var]->lock();
    item = consome_i(id_var);
    mtx_i[id_var]->unlock();
    std::cout << "\t\tConsumer consumed " << item << std::endl;
    posicoes_vazias_i[id_var]->release();
    return item;
}

void Buffer_Circular::produz_b(bool i, int id_var) {
    this->dados_b[id_var-N_VAR_I][this->in_b[id_var-N_VAR_I]] = i;
    this->in_b[id_var-N_VAR_I] = (this->in_b[id_var-N_VAR_I] + 1) % N_VEC;
}

bool Buffer_Circular::consome_b(int id_var) {
    bool item;
    item = this->dados_b[id_var-N_VAR_I][this->out_b[id_var-N_VAR_I]];
    this->out_b[id_var-N_VAR_I] = (this->out_b[id_var-N_VAR_I] + 1) % N_VEC;
    return item;
}

void Buffer_Circular::produtor_b(bool i, int id_var) {
    posicoes_vazias_b[id_var-N_VAR_I]->acquire(); 
    mtx_b[id_var-N_VAR_I]->lock();
    produz_b(i, id_var);
    mtx_b[id_var-N_VAR_I]->unlock();
    std::cout << "Producer produced " << i << std::endl;
    posicoes_cheias_b[id_var-N_VAR_I]->release();  
}

bool Buffer_Circular::consumidor_b(int id_var) {
    bool item;
    posicoes_cheias_b[id_var-N_VAR_I]->acquire();
    mtx_b[id_var-N_VAR_I]->lock();
    item = consome_b(id_var);
    mtx_b[id_var-N_VAR_I]->unlock();
    std::cout << "\t\tConsumer consumed " << item << std::endl;
    posicoes_vazias_b[id_var-N_VAR_I]->release();
    return item;
}
    //CRIAR LÓGICA NOS OUTROS MODULOS PARA QUE, SE NAO TIVER NOVAS INFORMACOES PRA ENVIAR PRO BUFFER EM UM NOVO CICLO,
    //A INFORMAÇÃO ANTERIOR SERÁ REPETIDA. ASSIM, O BUFFER NÃO FICARÁ MUITO TEMPO VAZIO E SEMPRE SERÁ ATUALIZADO. A TAXA
    //DE ATUALIZAÇÃO DOS PRODUTORES SERÁ MAIOR QUE A DOS CONSUMIDORES PARA EVITAR QUE FIQUEM ESPERANDO.