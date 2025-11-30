//IDEIA: STRUCT COM VARIOS VETORES DE DIFERENTES TIPOS, CADA UM SERÁ REFERENTE A UMA VARIÁVEL. TEREMOS 
//MAIS DE UM CONSUMIDOR PARA CADA VARIÁVEL. O TEMPO DE PERIODICIDADE PARA CADA TAREFA CONSUMIDORA DEVE ESTAR
//ALINHADO COM O TEMPO DE PERIODICIDADE PARA CADA TAREFA PRODUTORA, PARA QUE OS CONSUMIDORES NAO PEGUEM 
//INFORMACOES DESATUALIZADAS.

/*Cada variável terá um ID: 
    i_posicao_x   - 0
    i_posicao_y   - 1
    i_angulo_x    - 2
    c_acelera     - 3
    c_direita     - 4
    c_esquerda    - 5
    setpoint_vel  - 6   // setpoint de VELOCIDADE
    setpoint_ang_x- 7   // setpoint de ÂNGULO

    e_defeito     - 8
    e_automatico  - 9
    c_automatico  - 10
    c_man         - 11
    c_rearme      - 12

Cada tarefa também terá um ID - serve para garantir que a mesma tarefa não consuma a mesma posição duas vezes.
    logica_comando    0
    coletor_dados     1
    controle_navegacao 2
    planejamento_rota 3
*/

//CRIAR UM BUFFER NO MAIN

//PASSAR COMO PARÂMETRO NAS OUTRAS FUNÇÕES ID_SENSOR.

#include "buffer.h"
#include <iostream>
#include <iomanip> // >>> CORREÇÃO: necessário para std::setprecision

// ============================================================================
// Implementação do Buffer_Circular
// ============================================================================

Buffer_Circular::Buffer_Circular() {
    // Semáforos de "posições vazias" e mutex por variável (INT)
    for (int i = 0; i < N_VAR_I; ++i) {
        this->posicoes_vazias_i.push_back(
            std::make_unique<std::counting_semaphore<N_VEC>>(N_VEC)
        );
        this->mtx_i.push_back(std::make_unique<std::mutex>());
    }

    // Semáforos de "posições vazias" e mutex por variável (BOOL)
    for (int i = 0; i < N_VAR_B; ++i) {
        this->posicoes_vazias_b.push_back(
            std::make_unique<std::counting_semaphore<N_VEC>>(N_VEC)
        );
        this->mtx_b.push_back(std::make_unique<std::mutex>());
    }

    /*Cada sensor tem seu número de consumidores definido, de acordo com a seguinte tabela:
      (float/int)
        i_posicao_x   - produtor: tratamento de sensores
                        consumidores: controle de navegação, coletor de dados, planejamento de rota
        i_posicao_y   - produtor: tratamento de sensores
                        consumidores: controle de navegação, coletor de dados, planejamento de rota
        i_angulo_x    - produtor: tratamento de sensores
                        consumidores: controle de navegação, coletor de dados, planejamento de rota
        c_acelera     - produtores: coletor de dados / controle de navegação
                        consumidores: coletor de dados, lógica de comando
        c_direita     - produtores: coletor de dados / controle de navegação
                        consumidores: coletor de dados, lógica de comando
        c_esquerda    - produtores: coletor de dados / controle de navegação
                        consumidores: coletor de dados, lógica de comando
        setpoint_vel  - produtor: planejamento de rota
                        consumidor: controle de navegação
        setpoint_ang_x- produtor: planejamento de rota
                        consumidor: controle de navegação

      (bool)
        e_defeito     - produtor: lógica de comando
                        consumidores: coletor de dados, planejamento de rota
        e_automatico  - produtor: lógica de comando
                        consumidores: coletor de dados, planejamento de rota, controle de navegação
        c_automatico  - produtor: coletor de dados
                        consumidores: coletor de dados, lógica de comando
        c_man         - produtor: coletor de dados
                        consumidores: coletor de dados, lógica de comando
        c_rearme      - produtor: coletor de dados
                        consumidores: coletor de dados, lógica de comando
    */

    // Quantidade de consumidores por variável (INT)
    this->num_consumidores_i = {3, 3, 3, 2, 2, 2, 1, 1};

    // Quantidade de consumidores por variável (BOOL)
    this->num_consumidores_b = {2, 3, 2, 2, 2};

    // >>> CORREÇÃO: estrutura de semáforos para POSIÇÕES CHEIAS
    // Antes: 1 semáforo por variável (compartilhado por todos os consumidores),
    //        o que podia causar consumo “adiantado” de um consumidor.
    // Agora: 1 semáforo POR VARIÁVEL e POR CONSUMIDOR, garantindo que
    //        cada consumidor só anda quando tiver realmente um novo item
    //        disponível para ele.
    // ------------------------------------------------------------------------
    posicoes_cheias_i.resize(N_VAR_I);
    for (int idx = 0; idx < N_VAR_I; ++idx) {
        for (int c = 0; c < N_CONSUMIDORES; ++c) { // MAX_CONSUMIDORES definido em buffer.h
            posicoes_cheias_i[idx].push_back(
                std::make_unique<std::counting_semaphore<1>>(0)
            );
        }
    }

    posicoes_cheias_b.resize(N_VAR_B);
    for (int idx = 0; idx < N_VAR_B; ++idx) {
        for (int c = 0; c < N_CONSUMIDORES; ++c) {
            posicoes_cheias_b[idx].push_back(
                std::make_unique<std::counting_semaphore<1>>(0)
            );
        }
    }
}

// ============================================================================
// INT
// ============================================================================

void Buffer_Circular::produz_i(float i, int id_var) {
    this->dados_i[id_var][this->in_i[id_var]].valor = i;
    // >>> CORREÇÃO: garante que cada posição sabem quantos leitores ainda faltam
    dados_i[id_var][this->in_i[id_var]].leitores_restantes.store(num_consumidores_i[id_var]);
    this->in_i[id_var] = (this->in_i[id_var] + 1) % N_VEC;
}

float Buffer_Circular::consome_i(int id_var, int id_tarefa) {
    float item = this->dados_i[id_var][this->out_i[id_var][id_tarefa]].valor;

    // >>> CORREÇÃO: decremento do leitores_restantes ligado a esta posição
    int restante = --dados_i[id_var][this->out_i[id_var][id_tarefa]].leitores_restantes;

    out_i[id_var][id_tarefa] = (out_i[id_var][id_tarefa] + 1) % N_VEC;
    if (restante == 0) {
        // Último leitor liberando a posição
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

    // >>> CORREÇÃO: libera 1 permissão para CADA consumidor dessa variável
    for (int c = 0; c < num_consumidores_i[id_var]; ++c) {
        posicoes_cheias_i[id_var][c]->release();
    }
}

float Buffer_Circular::consumidor_i(int id_var, int id_tarefa) {
    float item;

    // >>> CORREÇÃO: cada tarefa-consumidora usa o seu próprio semáforo
    posicoes_cheias_i[id_var][id_tarefa]->acquire();

    mtx_i[id_var]->lock();
    item = consome_i(id_var, id_tarefa);
    mtx_i[id_var]->unlock();

    std::cout << "\t[CONSUMIDOR-I] ID " << id_var 
              << " (tarefa " << id_tarefa << ")"
              << " → valor consumido: " << item << std::endl;
    return item;
}

// ============================================================================
// BOOL
// ============================================================================

void Buffer_Circular::produz_b(bool i, int id_var) {
    int idx = id_var - N_VAR_I;
    this->dados_b[idx][this->in_b[idx]].valor = i;
    // >>> CORREÇÃO: idem para booleanos
    dados_b[idx][this->in_b[idx]].leitores_restantes.store(num_consumidores_b[idx]);
    this->in_b[idx] = (this->in_b[idx] + 1) % N_VEC;
}

bool Buffer_Circular::consome_b(int id_var, int id_tarefa) {
    bool item;
    int idx = id_var - N_VAR_I;

    item = this->dados_b[idx][this->out_b[idx][id_tarefa]].valor;

    // >>> CORREÇÃO: decremento de leitores_restantes para esta posição
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

    // >>> CORREÇÃO: libera 1 permissão para cada consumidor dessa variável booleana
    for (int c = 0; c < num_consumidores_b[idx]; ++c) {
        posicoes_cheias_b[idx][c]->release();
    }
}

bool Buffer_Circular::consumidor_b(int id_var, int id_tarefa) {
    int idx = id_var - N_VAR_I;
    bool item;

    // >>> CORREÇÃO: semáforo próprio para cada tarefa-consumidora
    posicoes_cheias_b[idx][id_tarefa]->acquire();

    mtx_b[idx]->lock();
    item = consome_b(id_var, id_tarefa);
    mtx_b[idx]->unlock();

    std::cout << "\t[CONS] Bool ID " << id_var 
              << " (tarefa " << id_tarefa << ")"
              << " → valor = " << (item ? "TRUE" : "FALSE") << std::endl;
    return item;
}

// ============================================================================
// Debug: mostra o conteúdo bruto do buffer
// ============================================================================

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

// ============================================================================
// Pequeno teste de uso do buffer (apenas para depuração)
// ============================================================================

