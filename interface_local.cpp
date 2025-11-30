#include "interface_local.h"

#include <iostream>
#include <thread>
#include <string>
#include <unistd.h> 
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <mutex>
#include <cmath>     
#include <algorithm>  
#include <cstring>    

using namespace std;

// Estas variáveis guardam o último estado conhecido recebido do Coletor.
float local_pos_x = 0.0f;
float local_pos_y = 0.0f;
float local_angulo = 0.0f;
string local_estado = "DESCONHECIDO";
string local_status = "DESCONHECIDO";

// Mutex para proteger o acesso a essas variáveis
mutex mtx_dados_locais;

// Limites do Mapa
const float MAX_X = 100.0f;
const float MAX_Y = 100.0f;
const float MIN_VAL = 0.0f;

// Mantém ângulo entre 0 e 360
void normalizar_angulo(float &angulo) {
    angulo = fmod(angulo, 360.0f);
    if (angulo < 0) angulo += 360.0f;
}

// Mantém posição dentro do mapa
void limitar_posicao(float &x, float &y) {
    x = std::clamp(x, MIN_VAL, MAX_X);
    y = std::clamp(y, MIN_VAL, MAX_Y);
}

// Função simples para extrair valores do log
void parse_mensagem_log(string msg) {
    lock_guard<mutex> lock(mtx_dados_locais);
    
    
    // 1. Extrair Estado
    if (msg.find("ESTADO: AUTOMATICO") != string::npos) local_estado = "AUTOMATICO";
    else if (msg.find("ESTADO: MANUAL") != string::npos) local_estado = "MANUAL";
    
    // 2. Extrair Status
    if (msg.find("STATUS: NORMAL") != string::npos) local_status = "NORMAL";
    else if (msg.find("STATUS: COM_DEFEITO") != string::npos) local_status = "DEFEITO";

    // 3. Extrair Posição
    size_t pos_start = msg.find("POS: (");
    if (pos_start != string::npos) {
        size_t start_num = pos_start + 6; 
        size_t comma = msg.find(",", start_num);
        size_t end_num = msg.find(")", comma);
        
        if (comma != string::npos && end_num != string::npos) {
            try {
                string s_x = msg.substr(start_num, comma - start_num);
                string s_y = msg.substr(comma + 1, end_num - (comma + 1));
                
                float x = stof(s_x);
                float y = stof(s_y);
                
                limitar_posicao(x, y);
                
                local_pos_x = x;
                local_pos_y = y;
                
                // Se o coletor passar a enviar angulo, adicione aqui.

            } catch (...) {
            }
        }
    }
}

void tarefa_interface_local(Buffer_Circular* buffer,
                            atomic<bool>& running,
                            int &leitor_coletor_dados,
                            int &leitor_interface_local) {
    
    int envio_cmd = 0; 
    mutex mtx_envio_cmd;

    cout << "[Interface Local] Tarefa iniciada.\n";
    cout << "-------------------------------------------------\n";
    cout << "  COMANDOS DE CONTROLE: \n";
    cout << "  a = Automatico | m = Manual | r = Rearme\n";
    cout << "  w/s = Acelerar/Frear | d/e = Dir/Esq\n";
    cout << "-------------------------------------------------\n";
    cout << "  VISUALIZACAO: \n";
    cout << "  p = Imprimir Status Atual (Sob Demanda)\n";
    cout << "-------------------------------------------------\n";

    // Thread 1: Lê teclado
    thread t_recebe_cmd(
        thread_recebe_cmd,
        buffer,
        ref(running),
        ref(leitor_coletor_dados),
        ref(envio_cmd),
        ref(mtx_envio_cmd)
    );

    // Thread 2: Lê pipe do coletor em background
    thread t_exibe_msg(
        thread_exibe_msg,
        buffer,
        ref(running),
        ref(leitor_interface_local),
        ref(envio_cmd),
        ref(mtx_envio_cmd)
    );

    t_recebe_cmd.join();
    t_exibe_msg.join();

    cout << "[Interface Local] Tarefa terminada." << endl;
}

void thread_recebe_cmd(Buffer_Circular* /*buffer*/,
                       atomic<bool>& running,
                       int &leitor_coletor_dados,
                       int &envio_cmd,
                       mutex &mtx_envio_cmd) {
    char comando;
    string msg;

    while (running) {
        cout << "> "; // Prompt
        cin >> comando;

        if (!running) break;

        // --- COMANDO DE VISUALIZAÇÃO ---
        if (comando == 'p' || comando == 'P') {
            // Acessa a memória local protegida e imprime
            lock_guard<mutex> lock(mtx_dados_locais);
            
            cout << "\n=== STATUS DO SISTEMA ===" << endl;
            cout << "Estado Operacional: " << local_estado << endl;
            cout << "Status de Falha:    " << local_status << endl;
            cout << "Posicao Atual:      (" << local_pos_x << ", " << local_pos_y << ")" << endl;
            cout << "Angulo (Norte=0):   " << local_angulo << " graus" << endl;
            cout << "=========================\n" << endl;
            
            continue; // Volta para o loop, não envia nada pro pipe
        }

        bool comando_valido = true;
        switch (comando) {
            case 'a': msg = "automatico"; break;
            case 'm': msg = "manual"; break;
            case 'r': msg = "rearme"; break;
            case 'w': msg = "acel_pos"; break;
            case 's': msg = "acel_neg"; break;
            case 'd': msg = "dir_dir"; break;
            case 'e': msg = "dir_esq"; break;
            default:
                cout << "[Interface] Comando invalido. Use 'p' para status.\n";
                comando_valido = false;
        }

        if (comando_valido) {
            write(leitor_coletor_dados, msg.c_str(), msg.size());
            cout << "[Interface] Comando enviado: " << msg << endl;
        }
    }
}


void thread_exibe_msg(Buffer_Circular* /*buffer*/,
                      atomic<bool>& running,
                      int &leitor_interface_local,
                      int &envio_cmd,
                      mutex &mtx_envio_cmd) {

    char buff[512]; 

    while (running) {
        // Lê do pipe (bloqueante, eficiente)
        ssize_t n = read(leitor_interface_local, buff, sizeof(buff) - 1);
        
        if (n > 0) {
            buff[n] = '\0';
            string mensagem_recebida(buff);

            // Processa a mensagem para atualizar as variáveis locais
            parse_mensagem_log(mensagem_recebida);
            
        }
        else {
            // espera um pouco para não fritar CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}