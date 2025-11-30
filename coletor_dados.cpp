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

using namespace std;

// FLOAT/INT
#define ID_I_POS_X      0
#define ID_I_POS_Y      1
#define ID_I_ANG_X      2
#define ID_C_ACELERA    3
#define ID_C_DIREITA    4
#define ID_C_ESQUERDA   5
// BOOL (IDs Globais)
#define ID_E_DEFEITO    8
#define ID_E_AUTOMATICO 9
#define ID_C_AUTOMATICO 10
#define ID_C_MANUAL     11
#define ID_C_REARME     12

// ID DA TAREFA (Conforme seu comentário: coletor_dados = 1)
#define ID_TAREFA_COLETOR 1

// Inicializado com 0 (bloqueado)
static std::counting_semaphore<1024> sem_novos_dados(0);

// Mutex para proteger as variáveis de comando
std::mutex mtx_dados_cmd;

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

// ============================================================================
// TAREFA PRINCIPAL
// ============================================================================
void tarefa_coletor_dados(Buffer_Circular* buffer,
                          atomic<bool>& running,
                          int id_caminhao,
                          int &leitor_coletor_dados,
                          int &leitor_interface_local) {

    cout << "[Coletor Dados] Tarefa iniciada. Monitorando..." << endl;

    // Variáveis de Estado (Lidas do Buffer p/ Log)
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    bool  modo_auto_lido = false;
    bool  defeito_lido   = false;

    // Variáveis de Comando (Escritas no Buffer p/ Controle)
    bool  automatico = false;   
    int   contador_rearme = 0;
    int   cmd_acel = 0;
    int   cmd_dir  = 0;
    bool  cmd_acel_pendente = false;
    bool  cmd_dir_pendente  = false;

    // 1. Thread Lê sensores do buffer (CONSUMIDOR)
    thread t_leitura(
        thread_leitura_sensores,
        buffer,
        ref(running),
        ref(pos_x),
        ref(pos_y),
        ref(modo_auto_lido),
        ref(defeito_lido)
    );

    // 2. Thread Grava em arquivo/Interface (LOGGER)
    thread t_armazena_log(
        thread_armazena,
        ref(running),
        id_caminhao,
        ref(pos_x),
        ref(pos_y),
        ref(modo_auto_lido),
        ref(defeito_lido),
        ref(leitor_interface_local)
    );

    // 3. Thread Envia comandos para o buffer (PRODUTOR - Loop Contínuo)
    thread t_envia(
        thread_envia_buffer,
        buffer,
        ref(running),
        ref(automatico),
        ref(contador_rearme),
        ref(cmd_acel),
        ref(cmd_dir),
        ref(cmd_acel_pendente),
        ref(cmd_dir_pendente)
    );

    // 4. Thread Interface: Escuta o usuário
    thread t_recebe(
        thread_recebe_pipe,
        ref(running),
        ref(leitor_coletor_dados),
        ref(automatico),
        ref(contador_rearme),
        ref(cmd_acel),
        ref(cmd_dir),
        ref(cmd_acel_pendente),
        ref(cmd_dir_pendente)
    );

    t_leitura.join();
    t_armazena_log.join();
    t_envia.join();
    t_recebe.join();

    cout << "[Coletor Dados] Tarefa terminada." << endl;
}

// ============================================================================
// THREADS AUXILIARES
// ============================================================================

// Lê continuamente do buffer original
void thread_leitura_sensores(Buffer_Circular* buffer, atomic<bool>& running, float &pos_x, float &pos_y, bool &modo_auto, bool &defeito) {
    while (running) {
        // Os métodos do buffer original (consumidor_i/b) já usam semáforos C++20 internamente
        // para bloquear se não houver dados. Isso está correto.
        
        pos_x = buffer->consumidor_i(ID_I_POS_X, ID_TAREFA_COLETOR);
        pos_y = buffer->consumidor_i(ID_I_POS_Y, ID_TAREFA_COLETOR);
        
        modo_auto = buffer->consumidor_b(ID_E_AUTOMATICO, ID_TAREFA_COLETOR);
        defeito   = buffer->consumidor_b(ID_E_DEFEITO,    ID_TAREFA_COLETOR);

        // Avisa a thread de log 
        sem_novos_dados.release();
    }
}

// Logger - Escreve quando recebe sinal
void thread_armazena(atomic<bool>& running,
                     int id_caminhao,
                     float &pos_x,
                     float &pos_y,
                     bool &modo_auto,
                     bool &defeito,
                     int &leitor_interface_local) {

    const std::string nome_arquivo = "log_caminhao_" + std::to_string(id_caminhao) + ".txt";

    while (running) {
        // Espera passivamente 
        sem_novos_dados.acquire();

        string estado_str  = (modo_auto ? "AUTOMATICO" : "MANUAL");
        string defeito_str = (defeito   ? "COM_DEFEITO" : "NORMAL");

        string msg = "[LOG] " + get_timestamp()
                     + " | CAMINHAO_" + to_string(id_caminhao)
                     + " | ESTADO: " + estado_str
                     + " | STATUS: " + defeito_str
                     + " | POS: (" + to_string(pos_x) + ", " + to_string(pos_y) + ")\n";

        // Envia para pipe
        write(leitor_interface_local, msg.c_str(), msg.size());

        // Grava em arquivo
        {
            ofstream arq(nome_arquivo, ios::app);
            if (arq.is_open()) arq << msg;
        }
        
        // Verifica falhas globais (usando mutexes definidos em variaveis.h)
        {
            lock_guard<mutex> lock(mtx_falha_eletrica);
            if (falha_eletrica_global) {
                string msg1 = "[LOG] FALHA: ELETRICA\n";
                write(leitor_interface_local, msg1.c_str(), msg1.size());
            }
        }
        // (Adicionar os outros if's de falha aqui conforme seu código original)
    }
}

// Produtor - Envia comandos para o buffer
void thread_envia_buffer(Buffer_Circular* buffer,
                         atomic<bool>& running,
                         bool &automatico,
                         int &contador_rearme,
                         int &cmd_acel,
                         int &cmd_dir,
                         bool &cmd_acel_pendente,
                         bool &cmd_dir_pendente) {

    while (running) {
        bool auto_local, manual_local, rearme_local = false;
        int acel_local = 0, dir_local = 0;
        bool enviar_acel = false, enviar_dir = false;
        
        // Copia dados protegidos
        {
            lock_guard<mutex> lock(mtx_dados_cmd);
            
            auto_local = automatico;
            manual_local = !automatico;

            if (contador_rearme > 0) {
                rearme_local = true;
                contador_rearme = 0; 
            }
            
            if (cmd_acel_pendente) {
                acel_local = cmd_acel;
                enviar_acel = true;
                cmd_acel_pendente = false;
            }
            
            if (cmd_dir_pendente) {
                dir_local = cmd_dir;
                enviar_dir = true;
                cmd_dir_pendente = false;
            }
        }
        
        // Escreve no buffer original
        buffer->produtor_b(auto_local,   ID_C_AUTOMATICO);
        buffer->produtor_b(manual_local, ID_C_MANUAL);
        buffer->produtor_b(rearme_local, ID_C_REARME);
        
        if (enviar_acel) {
            buffer->produtor_i((float)acel_local, ID_C_ACELERA);
        }
        
        if (enviar_dir) {
            if (dir_local > 0) { 
                buffer->produtor_i((float)dir_local, ID_C_DIREITA);
                buffer->produtor_i(0.0f, ID_C_ESQUERDA);
            } else {
                buffer->produtor_i((float)(-dir_local), ID_C_ESQUERDA); 
                buffer->produtor_i(0.0f, ID_C_DIREITA);
            }
        }
        
        std::this_thread::yield(); 
    }
}

// Interface com usuário (Pipe)
void thread_recebe_pipe(atomic<bool>& running,
                        int &leitor_coletor_dados,
                        bool &automatico,
                        int &contador_rearme,
                        int &cmd_acel,
                        int &cmd_dir,
                        bool &cmd_acel_pendente,
                        bool &cmd_dir_pendente) {

    char buff[256];
    while (running) {
        // Bloqueia aqui
        ssize_t n = read(leitor_coletor_dados, buff, sizeof(buff) - 1);
        
        if (n > 0) {
            buff[n] = '\0';

            lock_guard<mutex> lock(mtx_dados_cmd);

            if (strncmp(buff, "automatico", 10) == 0) {
                automatico = true;
            }
            else if (strncmp(buff, "manual", 6) == 0) {
                automatico = false;
            }
            else if (strncmp(buff, "rearme", 6) == 0) {
                contador_rearme = 1;
            }
            else if (strncmp(buff, "acel_pos", 8) == 0) {
                cmd_acel = 10;
                cmd_acel_pendente = true;
            }
            else if (strncmp(buff, "acel_neg", 8) == 0) {
                cmd_acel = -10;              
                cmd_acel_pendente = true;
            }
            else if (strncmp(buff, "dir_dir", 7) == 0) {
                cmd_dir = 10;                
                cmd_dir_pendente = true;
            }
            else if (strncmp(buff, "dir_esq", 7) == 0) {
                cmd_dir = -10;             
                cmd_dir_pendente = true;
            }
        }
    }
}