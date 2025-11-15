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

#include "buffer.h"
#include <iostream>

Buffer_Circular::Buffer_Circular() {
    for (int i = 0; i < N_VAR_I; ++i) {
        this->posicoes_vazias_i.push_back(make_unique<counting_semaphore<N_VEC>>(N_VEC));
        this->posicoes_cheias_i.push_back(make_unique<counting_semaphore<N_VEC>>(0));
        this->mtx_i.push_back(make_unique<mutex>());
    }
    for (int i = 0; i < N_VAR_B; ++i) {
        this->posicoes_vazias_b.push_back(make_unique<counting_semaphore<N_VEC>>(N_VEC));
        this->posicoes_cheias_b.push_back(make_unique<counting_semaphore<N_VEC>>(0));
        this->mtx_b.push_back(make_unique<mutex>());
    }
    /*Cada sensor tem seu número de consumidores definido, de acordo com a seguinte tabela:
    i_posicao_x - produtor tratamento de sensores, consumidores controle de navegação, coletor de dados, planejamento 
    de rota,
    i_posicao_y - produtor tratamento de sensores, consumidores controle de navegação, coletor de dados, planejamento 
    de rota,
    i_angulo_x - produtor tratamento de sensores, consumidores controle de navegação, coletor de dados, planejamento 
    de rota,
    c_acelera - produtor coletor de dados e controle de navegação, consumidores coletor de dados, logica de comando
    c_direita - produtor coletor de dados e controle de navegação, consumidores coletor de dados, logica de comando
    c_esquerda - produtor coletor de dados e controle de navegação, consumidores coletor de dados, logica de comando
    setpoint_pos_x - produtor planejamento de rota, consumidor controle de navegação
    setpoint_pos_y - produtor planejamento de rota, consumidor controle de navegação
    setpoint_ang_x - produtor planejamento de rota, consumidor controle de navegação

    e_defeito - produtor logica de comando, consumidores coletor de dados, planejamento de rota,
    e_automatico - produtor logica de comando, consumidores coletor de dados, planejamento de rota, controle de navegação
    c_automatico - produtor coletor de dados , consumidores coletor de dados, logica de comando
    c_man - produtor coletor de dados , consumidores coletor de dados, logica de comando
    c_rearme - produtor coletor de dados , consumidores coletor de dados, logica de comando
    */
    this->num_consumidores_i = {3,3,3,2,2,2,1,1};
    this->num_consumidores_b = {2,3,2,2,2};
}

void Buffer_Circular::produz_i(float i, int id_var) {
    this->dados_i[id_var][this->in_i[id_var]].valor = i;
    dados_i[id_var][this->in_i[id_var]].leitores_restantes.store(num_consumidores_i[id_var]);
    this->in_i[id_var] = (this->in_i[id_var] + 1) % N_VEC;
}

float Buffer_Circular::consome_i(int id_var, int id_tarefa) {

    float item = this->dados_i[id_var][this->out_i[id_var][id_tarefa]].valor;
    int restante = --dados_i[id_var][this->out_i[id_var][id_tarefa]].leitores_restantes;
    out_i[id_var][id_tarefa] = (out_i[id_var][id_tarefa] + 1) % N_VEC;
    if (restante == 0) {
        posicoes_vazias_i[id_var]->release();
    }
    return item;
}



void Buffer_Circular::produtor_i(float i, int id_var) {
    posicoes_vazias_i[id_var]->acquire(); 
    mtx_i[id_var]->lock();
    produz_i(i, id_var);
    mtx_i[id_var]->unlock();
    std::cout << "[PRODUTOR-I] ID " << id_var 
              << " → valor produzido: " << i << std::endl;
    posicoes_cheias_i[id_var]->release(num_consumidores_i[id_var]);  
}

float Buffer_Circular::consumidor_i(int id_var, int id_tarefa) {
    float item;
    posicoes_cheias_i[id_var]->acquire();
    mtx_i[id_var]->lock();
    item = consome_i(id_var, id_tarefa);
    mtx_i[id_var]->unlock();
    std::cout << "\t[CONSUMIDOR-I] ID " << id_var 
              << " → valor consumido: " << item << std::endl;
    return item;
}

void Buffer_Circular::produz_b(bool i, int id_var) {
    int idx = id_var - N_VAR_I;
    this->dados_b[idx][this->in_b[idx]].valor = i;
    dados_b[idx][this->in_b[idx]].leitores_restantes.store(num_consumidores_b[idx]);
    this->in_b[idx] = (this->in_b[idx] + 1) % N_VEC;
}

bool Buffer_Circular::consome_b(int id_var, int id_tarefa) {
    bool item;
    int idx = id_var - N_VAR_I;
    item = this->dados_b[idx][this->out_b[idx][id_tarefa]].valor;
    int restante = --dados_b[idx][this->out_b[idx][id_tarefa]].leitores_restantes;
    out_b[idx][id_tarefa] = (out_b[idx][id_tarefa] + 1) % N_VEC;
    if (restante == 0) {
        posicoes_vazias_b[idx]->release();
    }
    return item;
}

void Buffer_Circular::produtor_b(bool i, int id_var) {
    int idx = id_var - N_VAR_I;
    posicoes_vazias_b[idx]->acquire(); 
    mtx_b[idx]->lock();
    produz_b(i, id_var);
    mtx_b[idx]->unlock();
    std::cout << "[PROD] Bool ID " << id_var 
              << " → valor = " << (i ? "TRUE" : "FALSE") << std::endl;
    posicoes_cheias_b[idx]->release(num_consumidores_b[idx]);  
}

bool Buffer_Circular::consumidor_b(int id_var, int id_tarefa) {
    int idx = id_var - N_VAR_I;
    bool item;
    posicoes_cheias_b[idx]->acquire();
    mtx_b[idx]->lock();
    item = consome_b(id_var, id_tarefa);
    mtx_b[idx]->unlock();
    std::cout << "\t[CONS] Bool ID " << id_var 
              << " → valor = " << (item ? "TRUE" : "FALSE") << std::endl;
    return item;
}
void Buffer_Circular::mostrar_buffer() {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n========== ESTADO ATUAL DO BUFFER ==========\n";

    // Float
    for (int id = 0; id < N_VAR_I; ++id) {
        std::lock_guard<std::mutex> lock(*mtx_i[id]);
        std::cout << "[FLOAT] Variável ID " << id << " → ";
        for (int j = 0; j < N_VEC; ++j) {
            std::cout << dados_i[id][j].valor << " ";
        }
        std::cout << "\n";
    }

    // Booleanos
    for (int id = 0; id < N_VAR_B; ++id) {
        std::lock_guard<std::mutex> lock(*mtx_b[id]);
        std::cout << "[BOOL] Variável ID " << id + N_VAR_I << " → ";
        for (int j = 0; j < N_VEC; ++j) {
            std::cout << (dados_b[id][j].valor ? "T " : "F ");
        }
        std::cout << "\n";
    }

    std::cout << "===========================================\n";
}
    //CRIAR LÓGICA NOS OUTROS MODULOS PARA QUE, SE NAO TIVER NOVAS INFORMACOES PRA ENVIAR PRO BUFFER EM UM NOVO CICLO,
    //A INFORMAÇÃO ANTERIOR SERÁ REPETIDA. ASSIM, O BUFFER NÃO FICARÁ MUITO TEMPO VAZIO E SEMPRE SERÁ ATUALIZADO. A TAXA
    //DE ATUALIZAÇÃO DOS PRODUTORES SERÁ MAIOR QUE A DOS CONSUMIDORES PARA EVITAR QUE FIQUEM ESPERANDO.
int main(){
    Buffer_Circular buffer;
    buffer.produtor_i(10,0);
    buffer.mostrar_buffer();
    buffer.produtor_i(20,0);
    buffer.mostrar_buffer();
    buffer.produtor_i(30,0);
    buffer.mostrar_buffer();
    buffer.produtor_i(40,0);
    buffer.mostrar_buffer();

    buffer.consumidor_i(0,0);
    buffer.mostrar_buffer();
    buffer.consumidor_i(0,1);
    buffer.mostrar_buffer();
    buffer.produtor_i(60,0);
    buffer.mostrar_buffer();
    buffer.consumidor_i(0,1);
    buffer.mostrar_buffer();
    buffer.consumidor_i(0,0);
    buffer.mostrar_buffer();
    buffer.consumidor_i(0,2);
    buffer.mostrar_buffer();
    buffer.produtor_i(70,0);
    buffer.mostrar_buffer();


}