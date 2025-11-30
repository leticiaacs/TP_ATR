#include "coletor_dados.h"
#include "interface_local.h"
#include "variaveis.h"
#include "buffer.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore> 
#include <limits>
#include <fstream>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory> 

using namespace std;

#define NUM_CAMINHOES 200 

#define ID_TAREFA_COLETOR 1


// Vetor de Mutexes (um para cada caminhão)
static vector<unique_ptr<std::mutex>> mtx_dados_cmd_vec;

// Flag para garantir inicialização única
static std::once_flag flag_init_sinc;

// Função para inicializar os vetores de sincronização
void inicializar_sincronizacao() {
    mtx_dados_cmd_vec.clear();
    
    for(int i = 0; i < NUM_CAMINHOES; i++) {
        // Cria mutex
        mtx_dados_cmd_vec.push_back(make_unique<std::mutex>());
    }
    cout << "[Sistema] Sincronizacao inicializada para " << NUM_CAMINHOES << " caminhoes." << endl;
}

// Traz data e hora
string get_timestamp() {
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return string(buffer);
}

// TAREFA PRINCIPAL
void tarefa_coletor_dados(Buffer_Circular* buffer,
                          atomic<bool>& running,
                          int id_caminhao,
                          int &leitor_coletor_dados,
                          int &leitor_interface_local) {

    // Garante que os vetores sejam criados apenas na primeira vez que rodar
    std::call_once(flag_init_sinc, inicializar_sincronizacao);

    if (id_caminhao < 0 || id_caminhao >= NUM_CAMINHOES) {
        cerr << "[Erro] ID Caminhao " << id_caminhao << " invalido." << endl;
        return;
    }

    cout << "[Coletor " << id_caminhao << "] Tarefa iniciada." << endl;

    // Variáveis Locais (Estado)
    // Precisam ser mantidas vivas enquanto as threads rodarem
    // Como esta função 'tarefa_coletor_dados' bloqueia no join() final, as variáveis locais são seguras.
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float angulo = 0.0f;
    bool  modo_auto_lido = false;
    bool  defeito_lido   = false;

    // Variáveis Locais (Comando)
    bool  automatico = false;   
    int   contador_rearme = 0;
    int   cmd_acel = 0;
    int   cmd_dir  = 0;
    bool  cmd_acel_pendente = false;
    bool  cmd_dir_pendente  = false;
    // 1. Thread Leitura
    thread t_leitura_posx(
        thread_leitura_sensores,
        buffer,
        ref(running),
        id_caminhao,
        ref(pos_x),
    );

      thread t_leitura_posy(
        thread_leitura_sensores,
        buffer,
        ref(running),
        id_caminhao,
        ref(pos_y),
    );
      thread t_leitura_ang(
        thread_leitura_sensores,
        buffer,
        ref(running),
        id_caminhao,
        ref(angulo),
    );
      thread t_leitura_modo_auto(
        thread_leitura_sensores,
        buffer,
        ref(running),
        id_caminhao,
        ref(modo_auto_lido),
    );
      thread t_leitura_defeito(
        thread_leitura_sensores,
        buffer,
        ref(running),
        id_caminhao,
        ref(defeito_lido)
    );
    // 2. Thread Logger
    thread t_armazena_log(
        thread_armazena,
        ref(running),
        id_caminhao,
        ref(pos_x),
        ref(pos_y),
        ref(angulo),
        ref(modo_auto_lido),
        ref(defeito_lido),
        ref(leitor_interface_local)
    );

    // 3. Thread Envio (Produtor)
    thread t_envia(
        thread_envia_buffer,
        buffer,
        ref(running),
        id_caminhao,
        ref(automatico),
        ref(contador_rearme),
        ref(cmd_acel),
        ref(cmd_dir),
        ref(cmd_acel_pendente),
        ref(cmd_dir_pendente)
    );

    // 4. Thread Interface (Recebe)
    thread t_recebe(
        thread_recebe_pipe,
        ref(running),
        id_caminhao,
        ref(leitor_coletor_dados),
        ref(automatico),
        ref(contador_rearme),
        ref(cmd_acel),
        ref(cmd_dir),
        ref(cmd_acel_pendente),
        ref(cmd_dir_pendente)
    );

    t_leitura_posx.join();
    t_leitura_posy.join();
    t_leitura_ang.join();
    t_leitura_modo_auto.join();
    t_leitura_defeito.join();
    t_armazena_log.join();
    t_envia.join();
    t_recebe.join();

    cout << "[Coletor " << id_caminhao << "] Encerrado." << endl;
}

void thread_leitura_sensores_posx(Buffer_Circular* buffer, atomic<bool>& running, int id, float &pos_x) {
    while (running) {

        pos_x = buffer->consumidor_i(ID_I_POS_X, ID_TAREFA_COLETOR);

    }
}

void thread_leitura_sensores_posx(Buffer_Circular* buffer, atomic<bool>& running, int id, float &pos_y) {
    while(running){
        pos_y = buffer->consumidor_i(ID_I_POS_Y, ID_TAREFA_COLETOR);

    }
}

void thread_leitura_sensores_posx(Buffer_Circular* buffer, atomic<bool>& running, int id, float &angulo) {
    while(running){
        angulo = buffer->consumidor_i(ID_I_ANG_X, ID_TAREFA_COLETOR);

    }
}

void thread_leitura_sensores_posx(Buffer_Circular* buffer, atomic<bool>& running, int id, bool &modo_auto) {
    while(running){
        modo_auto = buffer->consumidor_b(ID_E_AUTOMATICO, ID_TAREFA_COLETOR);

    }
}

void thread_leitura_sensores_posx(Buffer_Circular* buffer, atomic<bool>& running, int id, bool &defeito) {
    while(running){
        defeito   = buffer->consumidor_b(ID_E_DEFEITO,    ID_TAREFA_COLETOR);
    }
}

void thread_armazena(atomic<bool>& running, int id_caminhao, float &pos_x, float &pos_y, float &angulo, bool &modo_auto, bool &defeito, int &leitor_interface_local) {
    const string nome_arquivo = "log_caminhao_" + to_string(id_caminhao) + ".txt";

    while (running) {

        string estado_str  = (modo_auto ? "AUTOMATICO" : "MANUAL");
        string defeito_str = (defeito   ? "COM_DEFEITO" : "NORMAL");

        string msg = "[LOG] " + get_timestamp()
                     + " | CAMINHAO_" + to_string(id_caminhao)
                     + " | ESTADO: " + estado_str
                     + " | STATUS: " + defeito_str
                     + " | POS: (" + to_string(pos_x) + ", " + to_string(pos_y) + ")"
                     + " | ANG: " + to_string(angulo) + "\n";

        write(leitor_interface_local, msg.c_str(), msg.size());

        {
            ofstream arq(nome_arquivo, ios::app);
            if (arq.is_open()) { arq << msg; arq.close(); }
        }
        
        // Logs de falhas globais 
        {
            lock_guard<mutex> lock(mtx_falha_eletrica);
            if (falha_eletrica_global) {
                string msg1 = "[LOG] FALHA: ELETRICA\n";
                write(leitor_interface_local, msg1.c_str(), msg1.size());
            }
        }
    }
}

void thread_envia_buffer(Buffer_Circular* buffer, atomic<bool>& running, int id, bool &automatico, int &contador_rearme, int &cmd_acel, int &cmd_dir, bool &cmd_acel_pendente, bool &cmd_dir_pendente) {
    while (running) {
        bool auto_local, manual_local, rearme_local = false;
        int acel_local = 0, dir_local = 0;
        bool enviar_acel = false, enviar_dir = false;
        
        {
            // Usa o mutex específico do vetor
            lock_guard<mutex> lock(*mtx_dados_cmd_vec[id]);
            
            auto_local = automatico;
            manual_local = !automatico;
            if (contador_rearme > 0) { rearme_local = true; contador_rearme = 0; }
            if (cmd_acel_pendente) { acel_local = cmd_acel; enviar_acel = true; cmd_acel_pendente = false; }
            if (cmd_dir_pendente) { dir_local = cmd_dir; enviar_dir = true; cmd_dir_pendente = false; }
        }

        buffer->produtor_b(auto_local,   ID_C_AUTOMATICO);
        buffer->produtor_b(manual_local, ID_C_MANUAL);
        buffer->produtor_b(rearme_local, ID_C_REARME);
        
        if (enviar_acel) buffer->produtor_i((float)acel_local, ID_C_ACELERA);
        if (enviar_dir) {
            if (dir_local > 0) { 
                buffer->produtor_i((float)dir_local, ID_C_DIREITA); buffer->produtor_i(0.0f, ID_C_ESQUERDA);
            } else {
                buffer->produtor_i((float)(-dir_local), ID_C_ESQUERDA); buffer->produtor_i(0.0f, ID_C_DIREITA);
            }
        }
        std::this_thread::yield(); 
    }
}

void thread_recebe_pipe(atomic<bool>& running, int id, int &leitor_coletor_dados, bool &automatico, int &contador_rearme, int &cmd_acel, int &cmd_dir, bool &cmd_acel_pendente, bool &cmd_dir_pendente) {
    char buff[256];
    while (running) {
        ssize_t n = read(leitor_coletor_dados, buff, sizeof(buff) - 1);
        if (n > 0) {
            buff[n] = '\0';
            
            // Usa o mutex específico do vetor
            lock_guard<mutex> lock(*mtx_dados_cmd_vec[id]);

            if (strncmp(buff, "automatico", 10) == 0) automatico = true;
            else if (strncmp(buff, "manual", 6) == 0) automatico = false;
            else if (strncmp(buff, "rearme", 6) == 0) contador_rearme = 1;
            else if (strncmp(buff, "acel_pos", 8) == 0) { cmd_acel = 10; cmd_acel_pendente = true; }
            else if (strncmp(buff, "acel_neg", 8) == 0) { cmd_acel = -10; cmd_acel_pendente = true; }
            else if (strncmp(buff, "dir_dir", 7) == 0) { cmd_dir = 10; cmd_dir_pendente = true; }
            else if (strncmp(buff, "dir_esq", 7) == 0) { cmd_dir = -10; cmd_dir_pendente = true; }
        }
    }
}