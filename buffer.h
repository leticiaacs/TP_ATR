//IDEIA: STRUCT COM VARIOS VETORES DE DIFERENTES TIPOS, CADA UM SERÁ REFERENTE A UMA VARIÁVEL. TEREMOS 
//MAIS DE UM CONSUMIDOR PARA CADA VARIÁVEL. O TEMPO DE PERIODICIDADE PARA CADA TAREFA CONSUMIDORA DEVE ESTAR
//ALINHADO COM O TEMPO DE PERIODICIDADE PARA CADA TAREFA PRODUTORA, PARA QUE OS CONSUMIDORES NAO PEGUEM 
//INFORMACOES DESATUALIZADAS.

/*Cada variável terá um ID: 
    i_posicao_x    - 0
    i_posicao_y    - 1
    i_angulo_x     - 2
    c_acelera      - 3
    c_direita      - 4
    c_esquerda     - 5
    setpoint_vel   - 6   // >>> CORREÇÃO: setpoint de VELOCIDADE, não posição
    setpoint_ang_x - 7   // >>> CORREÇÃO: setpoint de ÂNGULO

    e_defeito      - 8
    e_automatico   - 9
    c_automatico   - 10
    c_man          - 11
    c_rearme       - 12

Cada tarefa também terá um ID - serve para garantir que a mesma tarefa não consuma a mesma posição duas vezes.
    logica_comando     0
    coletor_dados      1
    controle_navegacao 2
    planejamento_rota  3
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

// >>> OBS: deixar using namespace std aqui evita ter que mexer em todos os .cpp
using namespace std;

//Número de colunas da matriz - número de posições de cada vetor
#define N_VEC 200

//Número de linhas da matriz dados_i - numero total de variáveis do tipo float/int (sensores, atuadores, comandos, estados)
#define N_VAR_I 8

//Número de linhas da matriz dados_b - numero total de variáveis do tipo bool (sensores, comandos, estados)
#define N_VAR_B 5

//Número de possíveis tarefas consumidoras (logica_comando, coletor_dados, controle_navegacao, planejamento_rota)
#define N_CONSUMIDORES 4   // >>> CORREÇÃO: este valor é usado para dimensionar out_i/out_b e os semáforos de consumidores

struct DadoInt {
    float valor = 0.0f;
    atomic<int> leitores_restantes{0};  // >>> CORREÇÃO: contador de leitores p/ cada posição
};

struct DadoBool {
    bool valor = false;
    atomic<int> leitores_restantes{0};  // >>> CORREÇÃO: idem para booleanos
};

class Buffer_Circular {
public:
    Buffer_Circular();

    void mostrar_buffer();

    // PRODUTORES / CONSUMIDORES de variáveis float/int
    void produtor_i(float i, int id_var);
    float consumidor_i(int id_var, int id_tarefa);

    // PRODUTORES / CONSUMIDORES de variáveis bool
    void produtor_b(bool i, int id_var);
    bool consumidor_b(int id_var, int id_tarefa);

private:
    // Matriz de dados INT: [variável][posição do buffer]
    DadoInt dados_i[N_VAR_I][N_VEC] = {0};

    // Matriz de dados BOOL: [variável][posição do buffer]
    DadoBool dados_b[N_VAR_B][N_VEC] = {false};

    // Índice de escrita (produtor) para cada variável INT
    int in_i[N_VAR_I] = {0};

    // Índice de escrita (produtor) para cada variável BOOL
    int in_b[N_VAR_B] = {0};

    // Índice de leitura (consumidor) para cada variável INT e cada tarefa consumidora
    // [variável][id_tarefa]
    int out_i[N_VAR_I][N_CONSUMIDORES] = {0};

    // Índice de leitura (consumidor) para cada variável BOOL e cada tarefa consumidora
    // [variável][id_tarefa]
    int out_b[N_VAR_B][N_CONSUMIDORES] = {0};

    // -------------------------------------------------------------------------
    // Semáforos e mutex para variáveis INT
    // -------------------------------------------------------------------------

    // posicoes_vazias_i[id_var] controla quantas posições LIVRES essa variável ainda tem
    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_vazias_i;

    // >>> CORREÇÃO IMPORTANTE:
    // Agora temos um vetor de semáforos de "posições cheias" POR VARIÁVEL E POR CONSUMIDOR:
    // posicoes_cheias_i[id_var][id_tarefa]
    // Assim, cada tarefa-consumidora só anda quando realmente existe um item novo pra ela.
    vector<vector<unique_ptr<counting_semaphore<1>>>> posicoes_cheias_i;

    // Um mutex por variável INT para proteger acesso concorrente ao vetor daquela variável
    vector<unique_ptr<mutex>> mtx_i;

    // Número de consumidores para cada variável INT (tamanho = N_VAR_I)
    vector<int> num_consumidores_i;

    // -------------------------------------------------------------------------
    // Semáforos e mutex para variáveis BOOL
    // -------------------------------------------------------------------------

    // posicoes_vazias_b[idx] controla quantas posições LIVRES a variável booleana (idx) ainda tem
    vector<unique_ptr<counting_semaphore<N_VEC>>> posicoes_vazias_b;

    // >>> CORREÇÃO IMPORTANTE:
    // Mesmo esquema de INT: um semáforo de "posições cheias" por variável e por consumidor:
    // posicoes_cheias_b[idx][id_tarefa]
    vector<vector<unique_ptr<counting_semaphore<1>>>> posicoes_cheias_b;

    // Um mutex por variável BOOL
    vector<unique_ptr<mutex>> mtx_b;

    // Número de consumidores para cada variável BOOL (tamanho = N_VAR_B)
    vector<int> num_consumidores_b;

    // -------------------------------------------------------------------------
    // Funções internas de produção/consumo
    // -------------------------------------------------------------------------

    void produz_i(float i, int id_var);
    float consome_i(int id_var, int id_tarefa);

    void produz_b(bool i, int id_var);
    bool consome_b(int id_var, int id_tarefa);
};

#endif // BUFFER_H
