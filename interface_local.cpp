#include "interface_local.h"
#include "coletor_dados.h"
#include "monitoramento_de_falhas.h"
#include "logica_comando.h"
#include "variaveis.h"
#include <iostream>
#include <thread>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

using namespace std;

void tarefa_interface_local(Buffer_Circular* buffer, atomic<bool>& running, int &leitor_coletor_dados, int &leitor_interface_local) {
    int envio_cmd = 0;
    mutex mtx_envio_cmd;

    cout << "[Interface Local] Tarefa iniciada. Para enviar comandos, a qualquer momento digite 'a' (Auto), 'm' (Manual), 'r' (Rearme) e pressione Enter:" << endl;
    // Threads para cada sensor
    thread t_recebe_cmd(thread_recebe_cmd, ref(buffer), ref(running), ref(leitor_coletor_dados), ref(envio_cmd), ref(mtx_envio_cmd)); 
    thread t_exibe_msg(thread_exibe_msg, ref(buffer), ref(running), ref(leitor_interface_local), ref(envio_cmd), ref(mtx_envio_cmd)); 

    // Espera todas terminarem
    t_recebe_cmd.join();
    t_exibe_msg.join();

    cout << "[Interface Local] Tarefa terminada." << endl;
};

void thread_recebe_cmd(Buffer_Circular* buffer, atomic<bool>& running, int &leitor_coletor_dados, int &envio_cmd, mutex &mtx_envio_cmd) {
    char comando;
    string msg;

            while(running && cin >> comando){
                mtx_envio_cmd.lock();
                envio_cmd = 1;
                mtx_envio_cmd.unlock();
                if (!running) break;
                        // Este loop le entradas do usuário                
                switch(comando) {
                    case 'a':{
                        // Produz o COMANDO para o modo automático
                        msg = "automatico";
                        ssize_t w = write(leitor_coletor_dados, msg.c_str(), msg.size());
                        cout << "[Interface Local] MODO AUTOMÁTICO enviado." << endl;
                        cout << "Para enviar comandos, a qualquer momento digite 'a' (Auto), 'm' (Manual), 'r' (Rearme) e pressione Enter:" << endl;
                        envio_cmd = 0;
                        break;
                    }
                    case 'm':{
                        // Produz o COMANDO para o modo manual
                        msg = "manual";
                        ssize_t w = write(leitor_coletor_dados, msg.c_str(), msg.size());
                        cout << "[Interface Local] MODO MANUAL enviado." << endl;
                        cout << "Para enviar comandos, a qualquer momento digite 'a' (Auto), 'm' (Manual), 'r' (Rearme) e pressione Enter:." << endl;
                        envio_cmd = 0;
                        break;
                    }
                    case 'r':{
                        // Produz o COMANDO de rearme
                        msg = "rearme";
                        ssize_t w = write(leitor_coletor_dados, msg.c_str(), msg.size());
                        cout << "[Interface Local] REARME DE FALHA enviado." << endl;
                        cout << "Para enviar comandos, a qualquer momento digite 'a' (Auto), 'm' (Manual), 'r' (Rearme) e pressione Enter:" << endl;
                        envio_cmd = 0;
                        break;
                    }
                    default:
                    cout << "[Interface Local] Comando '" << comando << "' desconhecido." << endl;
                    cout << "Para enviar comandos, a qualquer momento digite 'a' (Auto), 'm' (Manual), 'r' (Rearme) e pressione Enter:" << endl;
                    envio_cmd = 0;
                
            }


        }
};  


void thread_exibe_msg(Buffer_Circular* buffer, atomic<bool>& running, int &leitor_interface_local, int &envio_cmd,  mutex &mtx_envio_cmd) {

    char buff[256];
    while(running){
        mtx_envio_cmd.lock();
        if(envio_cmd == 0){
            mtx_envio_cmd.unlock();
            ssize_t n = read(leitor_interface_local, buff, sizeof(buff)-1);
            if (n <= 0) break;
            buff[n] = '\0';
            cout << "ESTADO CAMINHÕES DA MINA: " << buff << endl;
            this_thread::sleep_for(chrono::seconds(2));
        }else {
            mtx_envio_cmd.unlock();
        }
    }
};

/*int main(){

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

int flags;
flags = fcntl(leitor_coletor_dados[0], F_GETFL, 0);
fcntl(leitor_coletor_dados[0], F_SETFL, flags | O_NONBLOCK);

flags = fcntl(leitor_interface_local[0], F_GETFL, 0);
fcntl(leitor_interface_local[0], F_SETFL, flags | O_NONBLOCK);


std::thread t_coletor([&]() {
    tarefa_coletor_dados(&buffer, running, 1, leitor_coletor_dados[0], leitor_interface_local[1]);
});

std::thread t_interface([&]() {
    tarefa_interface_local(&buffer, running, leitor_coletor_dados[1], leitor_interface_local[0]);
});
 Logica_Comando logica;

// Thread 1 - Monitoramento de falhas
    std::thread t_monitor([&]() {
        tarefa_monitoramento_falhas(&buffer, running);
    });

    // Thread 2 - Lógica de comando
    std::thread t_logica([&]() {
        logica.logica_comando(buffer);
    });

    // -------------------------------
    // Simula o sistema rodando por 10 segundos
    // -------------------------------
    for (int i = 0; i < 70; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        /*if ((i == 1)||(i == 2)||(i == 3)||(i == 4) ){
            //buffer.produtor_b(true,10);
            //buffer.produtor_b(false,12);
            //buffer.consumidor_b(8,0);

        } /*else if ((i == 13)||(i == 14)||(i == 15)) {
            buffer.produtor_b(true,12);
            buffer.produtor_b(false,10);
            buffer.consumidor_b(8,0);

        } else {
            buffer.produtor_b(false,10);
            buffer.produtor_b(false,12);
            buffer.consumidor_b(8,0);
        }
        buffer.mostrar_buffer();


} 
t_coletor.join();
t_interface.join();
}*/