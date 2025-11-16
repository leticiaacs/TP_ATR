#include "coletor_dados.h"
#include "interface_local.h"
#include "variaveis.h"
#include <iostream>
#include <thread>
#include <chrono> 
#include <ctime> 
#include <unistd.h>   
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

// Função para pegar o timestamp atual formatado
string get_timestamp() {
    time_t rawtime;
    struct tm * timeinfo;
    char buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return string(buffer);
}

void tarefa_coletor_dados(Buffer_Circular* buffer, atomic<bool>& running, int id_caminhao, int &leitor_coletor_dados, int &leitor_interface_local) {
    
    cout << "[Coletor Dados] Tarefa iniciada. Monitorando..." << endl;
    //int id_caminhao = 1; // ID Fixo para este caminhão

    float pos_x ;
    float pos_y;
    bool modo_auto;
    bool defeito;
    bool automatico = false;
    int contador_rearme = 0;

    // Threads para cada sensor
    thread t_pos_x(thread_pos_x, ref(buffer), ref(running), ref(pos_x)); 
    thread t_pos_y(thread_pos_y, ref(buffer), ref(running), ref(pos_y)); 
    thread t_modo_auto(thread_modo_auto, ref(buffer), ref(running), ref(modo_auto)); 
    thread t_defeito(thread_defeito, ref(buffer), ref(running), ref(defeito)); 
    thread t_armazena(thread_armazena, ref(buffer), ref(running), ref(id_caminhao), ref(pos_x), ref(pos_y), ref(modo_auto), ref(defeito), ref(leitor_coletor_dados), ref(leitor_interface_local)); 
    thread t_envia_buffer(thread_envia_buffer, ref(buffer), ref(running), ref(leitor_coletor_dados), ref(leitor_interface_local), ref(automatico), ref(contador_rearme));
    thread t_recebe_pipe(thread_recebe_pipe, ref(buffer),ref(running), ref(leitor_coletor_dados), ref(automatico), ref(contador_rearme));    
    // Espera todas terminarem
    t_pos_x.join();
    t_pos_y.join();
    t_modo_auto.join();
    t_defeito.join();
    t_armazena.join();
    t_envia_buffer.join();
    t_recebe_pipe.join();

    cout << "[Coletor Dados] Tarefa terminada." << endl;
};

void thread_pos_x(Buffer_Circular* buffer, atomic<bool>& running, float &pos_x) {
    while (running) {
    
        // --- 1. CONSUMIR DADOS DE ESTADO ---
        // Consome dados dos Sensores
        pos_x = buffer->consumidor_i(ID_I_POS_X, 1);

        // valores "falsos" só para o 'cout' funcionar
        //int pos_x = -1;
        //int pos_y = -1;
        //bool modo_auto = false;
        //bool defeito = false;
    }

};


void thread_pos_y(Buffer_Circular* buffer, atomic<bool>& running, float &pos_y) {

    while(running){
 // --- 1. CONSUMIR DADOS DE ESTADO ---
        // Consome dados dos Sensores
        pos_y = buffer->consumidor_i(ID_I_POS_Y, 1);
    }

};


void thread_modo_auto(Buffer_Circular* buffer, atomic<bool>& running, bool &modo_auto) {
    while(running){
 // --- 1. CONSUMIR DADOS DE ESTADO ---
        // Consome dados dos Sensores
        modo_auto = buffer->consumidor_b(ID_E_AUTOMATICO, 1);
     }
};


void thread_defeito(Buffer_Circular* buffer, atomic<bool>& running, bool &defeito) {
    while(running){
 // --- 1. CONSUMIR DADOS DE ESTADO ---
        // Consome dados dos Sensores
        defeito = buffer->consumidor_b(ID_E_DEFEITO, 1);
    }
};

void thread_armazena(Buffer_Circular* buffer, atomic<bool>& running, int id_caminhao, float &pos_x, float &pos_y, bool &modo_auto, bool &defeito, int &leitor_coletor_dados, int &leitor_interface_local) {

    while(running){
            // --- 2. GERAR O LOG  ---
        string estado_str = (modo_auto ? "AUTOMATICO" : "MANUAL");
        string defeito_str = (defeito ? "COM_DEFEITO" : "NORMAL");

      string msg = "[LOG] " + get_timestamp()
                   + " | CAMINHAO_" + to_string(id_caminhao)
                   + " | ESTADO: " + estado_str
                   + " | STATUS: " + defeito_str
                   + " | POS: (" + to_string(pos_x) + ", " + to_string(pos_y) + ")\n";

        write(leitor_interface_local, msg.c_str(), msg.size());

        this_thread::sleep_for(chrono::seconds(2));
             
        mtx_falha_eletrica.lock();
        if(falha_eletrica_global == true){
        string msg1 =  "[LOG] " + get_timestamp()  
             + " | CAMINHAO_" + to_string(id_caminhao) 
             + " | PRESENÇA DE FALHA DETECTADA: FALHA ELÉTRICA";
        write(leitor_interface_local, msg1.c_str(), msg1.size());
        }
        mtx_falha_eletrica.unlock();

        mtx_falha_hidraulica.lock();
        if(falha_hidraulica_global == true){
        string msg2 =  "[LOG] " + get_timestamp()  
             + " | CAMINHAO_" + to_string(id_caminhao) 
             + " | PRESENÇA DE FALHA DETECTADA: FALHA HIDRÁULICA";
        write(leitor_interface_local, msg2.c_str(), msg2.size());
        }
        mtx_falha_hidraulica.unlock();

        mtx_falha_temp_alerta.lock();
        if(falha_temp_alerta_global == true){ 
        string msg3 =  "[LOG] " + get_timestamp()  
             + " | CAMINHAO_" + to_string(id_caminhao) 
             + " | PRESENÇA DE FALHA DETECTADA: TEMPERATURA MUITO ALTA - DEFEITO";
        write(leitor_interface_local, msg3.c_str(), msg3.size());
        }
        mtx_falha_temp_alerta.unlock();

        mtx_falha_temp_defeito.lock();
        if(falha_temp_defeito_global== true){
        string msg4 =  "[LOG] " + get_timestamp()  
             + " | CAMINHAO_" + to_string(id_caminhao) 
             + " | PRESENÇA DE FALHA DETECTADA: TEMPERATURA ALTA - ALERTA";
        write(leitor_interface_local, msg4.c_str(), msg4.size());
        }
        mtx_falha_temp_defeito.unlock();

        // Tarefa cíclica
        this_thread::sleep_for(chrono::seconds(2)); 
    }
};

void thread_envia_buffer(Buffer_Circular* buffer, atomic<bool>& running, int &leitor_coletor_dados, int &leitor_interface_local, bool &automatico, int &contador_rearme){
   while(running){
        this_thread::sleep_for(chrono::seconds(3)); 
        buffer->produtor_b(automatico, ID_C_AUTOMATICO);
        if((contador_rearme > 0)&&(contador_rearme <= 3)){
            buffer->produtor_b(true, ID_C_REARME);
            contador_rearme++;
        } else if (contador_rearme == 0){
            buffer->produtor_b(false, ID_C_REARME);
        }else {
            buffer->produtor_b(false, ID_C_REARME);
            contador_rearme = 0;
        }

    }
};

void thread_recebe_pipe(Buffer_Circular* buffer, atomic<bool>& running, int &leitor_coletor_dados, bool &automatico, int &contador_rearme){
   char buff[256];
    while(running){
        ssize_t n = read(leitor_coletor_dados, buff, sizeof(buff)-1);
        if (n>0){
            buff[n] = '\0';
            if(strncmp(buff,"automatico", 10)==0){
                automatico = true;
            } else if (strncmp(buff,"manual", 6)==0){
                automatico = false;
            }else if (strncmp(buff,"rearme", 6)==0){
                contador_rearme = 1;
            } 

        }
    }
};

int main() {

    Buffer_Circular buffer;
    atomic<bool> running(true);

    int leitor_coletor_dados[2];
int leitor_interface_local[2];

if (pipe(leitor_coletor_dados) < 0) {
    perror("pipe");
    exit(1);
}
if (pipe(leitor_interface_local) < 0) {
    perror("pipe");
    exit(1);
}


    std::thread t_interface([&]() {
    tarefa_interface_local(&buffer, running, leitor_coletor_dados[1], leitor_interface_local[0]);
});

    // Simulação de pipes (não serão usados no teste)

    int id_caminhao = 1;

    // ----- INICIA AS THREADS DO COLETOR -----
    thread t_coletor([&]() {
        tarefa_coletor_dados(
            &buffer, 
            running,
            id_caminhao,
leitor_coletor_dados[0], leitor_interface_local[1]
        );
    });

    // ------ THREAD QUE PRODUZ DADOS DE TESTE ------
    thread t_simulacao([&]() {
        float x = 0, y = 0;
        bool auto_mode = false;
        bool defeito = false;

        while (running) {
            buffer.produtor_i(x, ID_I_POS_X);
            buffer.produtor_i(y, ID_I_POS_Y);
            buffer.produtor_b(auto_mode, ID_E_AUTOMATICO);
            buffer.produtor_b(defeito, ID_E_DEFEITO);

            x += 1.0;
            y += 2.0;
            auto_mode = !auto_mode;
            defeito = !defeito;

            this_thread::sleep_for(chrono::milliseconds(500));
        }
    });

    // ------ LAÇO PRINCIPAL: MOSTRA BUFFER ------
    for (int i = 0; i < 20; i++) {
        cout << "\n------------------------\n";
        cout << "LOOP PRINCIPAL: ESTADO ATUAL DO BUFFER:\n";
        //buffer.mostrar_buffer();
        cout << "------------------------\n";
        sleep(1);
    }

    running = false;

    t_coletor.join();
    t_simulacao.join();

    cout << "Teste finalizado!" << endl;

    return 0;
}