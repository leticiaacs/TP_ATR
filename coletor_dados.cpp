#include "coletor_dados.h"
#include "variaveis.h"
#include <iostream>
#include <thread>
#include <chrono> 
#include <ctime> 

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

void tarefa_coletor_dados(Buffer_Circular* buffer, atomic<bool>& running, int id_caminhao) {
    
    cout << "[Coletor Dados] Tarefa iniciada. Monitorando..." << endl;
    //int id_caminhao = 1; // ID Fixo para este caminhão

    float pos_x ;
    float pos_y;
    bool modo_auto;
    bool defeito;

    // Threads para cada sensor
    thread t_pos_x(thread_pos_x, ref(buffer), ref(running), ref(pos_x)); 
    thread t_pos_y(thread_pos_y, ref(buffer), ref(running), ref(pos_y)); 
    thread t_modo_auto(thread_modo_auto, ref(buffer), ref(running), ref(modo_auto)); 
    thread t_defeito(thread_defeito, ref(buffer), ref(running), ref(defeito)); 
    thread t_armazena(thread_armazena, ref(buffer), ref(running), ref(id_caminhao), ref(pos_x), ref(pos_y), ref(modo_auto), ref(defeito)); 

        // Espera todas terminarem
    t_pos_x.join();
    t_pos_y.join();
    t_modo_auto.join();
    t_defeito.join();
    t_armazena.join();

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

void thread_armazena(Buffer_Circular* buffer, atomic<bool>& running, int id_caminhao, float &pos_x, float &pos_y, bool &modo_auto, bool &defeito) {

    while(running){
            // --- 2. GERAR O LOG  ---
        string estado_str = (modo_auto ? "AUTOMATICO" : "MANUAL");
        string defeito_str = (defeito ? "COM_DEFEITO" : "NORMAL");

        cout << "[LOG] " << get_timestamp() 
             << " | CAMINHAO_" << id_caminhao 
             << " | ESTADO: " << estado_str 
             << " | STATUS: " << defeito_str
             << " | POS: (" << pos_x << ", " << pos_y << ")" << endl;
        mtx_falha_eletrica.lock();
        if(falha_eletrica_global == true){
             cout << "[LOG] " << get_timestamp()  
             << " | CAMINHAO_" << id_caminhao 
             << " | PRESENÇA DE FALHA DETECTADA: FALHA ELÉTRICA" << endl;
        }
        mtx_falha_eletrica.unlock();

        mtx_falha_hidraulica.lock();
        if(falha_hidraulica_global == true){
             cout << "[LOG] " << get_timestamp()  
             << " | CAMINHAO_" << id_caminhao 
             << " | PRESENÇA DE FALHA DETECTADA: FALHA HIDRÁULICA" << endl;
        }
        mtx_falha_hidraulica.unlock();

        mtx_falha_temp_alerta.lock();
        if(falha_temp_alerta_global == true){
             cout << "[LOG] " << get_timestamp()  
             << " | CAMINHAO_" << id_caminhao 
             << " | PRESENÇA DE FALHA DETECTADA: TEMPERATURA MUITO ALTA - DEFEITO" << endl;
        }
        mtx_falha_temp_alerta.unlock();

        mtx_falha_temp_defeito.lock();
        if(falha_temp_defeito_global== true){
             cout << "[LOG] " << get_timestamp()  
             << " | CAMINHAO_" << id_caminhao 
             << " | PRESENÇA DE FALHA DETECTADA: TEMPERATURA ALTA ALERTA" << endl;
        }
        mtx_falha_temp_defeito.unlock();

        // Tarefa cíclica
        this_thread::sleep_for(chrono::seconds(2)); 
    }

};