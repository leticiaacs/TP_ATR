#include "interface_local.h"
#include "coletor_dados.h"
#include "variaveis.h"
#include <iostream>
#include <thread>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

void tarefa_interface_local(Buffer_Circular* buffer, atomic<bool>& running, int leitor_coletor_dados, int leitor_interface_local) {
    int envio_cmd = 0;
    mutex mtx_envio_cmd;

    cout << "[Interface Local] Tarefa iniciada. A qualquer momento, digite 1  + Enter para enviar comandos." << endl;
    // Threads para cada sensor
    thread t_recebe_cmd(thread_recebe_cmd, ref(buffer), ref(running), leitor_coletor_dados, ref(envio_cmd), ref(mtx_envio_cmd)); 
    thread t_exibe_msg(thread_exibe_msg, ref(buffer), ref(running), leitor_interface_local, ref(envio_cmd), ref(mtx_envio_cmd)); 

    // Espera todas terminarem
    t_recebe_cmd.join();
    t_exibe_msg.join();

    cout << "[Interface Local] Tarefa terminada." << endl;
};

void thread_recebe_cmd(Buffer_Circular* buffer, atomic<bool>& running, int leitor_coletor_dados, int &envio_cmd, mutex &mtx_envio_cmd) {
    string inicia_comando;
    char comando;

    //INSERIR COND_VAR PARA NÃO FICAR RODANDO ATOA O WHILE
            getline(cin, inicia_comando);

            while(running && inicia_comando == "1"){
                mtx_envio_cmd.lock();
                envio_cmd = 1;
                mtx_envio_cmd.unlock();
                if (!running) break;
                        // Este loop le entradas do usuário                
                cout << "Digite 'a' (Auto), 'm' (Manual), 'r' (Rearme) e pressione Enter:" << endl;

                switch(comando) {
                    case 'a':
                        // Produz o COMANDO para o modo automático
                        buffer->produtor_b(true, ID_C_AUTOMATICO);
                        cout << "[Interface Local] MODO AUTOMÁTICO enviado." << endl;
                        cout << "A qualquer momento, digite 1  + Enter para enviar comandos." << endl;
                        envio_cmd = 0;
                        break;
                    case 'm':
                        // Produz o COMANDO para o modo manual
                        buffer->produtor_b(true, ID_C_MANUAL);
                        cout << "[Interface Local] MODO MANUAL enviado." << endl;
                        cout << "A qualquer momento, digite 1  + Enter para enviar comandos." << endl;
                        envio_cmd = 0;
                        break;
                    case 'r':
                        // Produz o COMANDO de rearme
                        buffer->produtor_b(true, ID_C_REARME);
                        cout << "[Interface Local] REARME DE FALHA enviado." << endl;
                        cout << "A qualquer momento, digite 1  + Enter para enviar comandos." << endl;
                        envio_cmd = 0;
                        break;
                
                    default:
                    cout << "[Interface Local] Comando '" << comando << "' desconhecido." << endl;
                    cout << "A qualquer momento, digite 1  + Enter para enviar comandos." << endl;
                    envio_cmd = 0;
                
            }


        }
};  


void thread_exibe_msg(Buffer_Circular* buffer, atomic<bool>& running, int leitor_interface_local, int &envio_cmd,  mutex &mtx_envio_cmd) {

    char buff[256];
    while(running){
        mtx_envio_cmd.lock();
        if(envio_cmd == 0){
            mtx_envio_cmd.unlock();
            ssize_t n = read(leitor_interface_local, buff, sizeof(buff)-1);
            if (n <= 0) break;
            buff[n] = '\0';
            cout << "ESTADO CAMINHÕES DA MINA: " << buff;
            this_thread::sleep_for(chrono::seconds(2));
        }else {
            mtx_envio_cmd.unlock();
        }
    }
};

int main(){

int leitor_coletor_dados[2];
int leitor_interface_local[2];

Buffer_Circular buffer;
std::atomic<bool> running(true);

if (pipe(leitor_coletor_dados) < 0) {
    perror("pipe");
    exit(1);
}
if (pipe(leitor_interface_local) < 0) {
    perror("pipe");
    exit(1);
}

std::thread t_coletor([&]() {
    tarefa_coletor_dados(&buffer, running, 1, leitor_coletor_dados[0], leitor_interface_local[1]);
});

std::thread t_interface([&]() {
    tarefa_interface_local(&buffer, running, leitor_coletor_dados[1], leitor_interface_local[0]);
});


t_coletor.join();
t_interface.join();

}