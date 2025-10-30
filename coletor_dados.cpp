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

void tarefa_coletor_dados(Buffer_Circular* buffer, atomic<bool>& running) {
    
    cout << "[Coletor Dados] Tarefa iniciada. Monitorando..." << endl;
    int id_caminhao = 1; // ID Fixo para este caminhão

    while (running) {
    
        // --- 1. CONSUMIR DADOS DE ESTADO ---
        // Consome dados dos Sensores
        buffer->consumidor_i(ID_I_POS_X);
        buffer->consumidor_i(ID_I_POS_Y);

        buffer->consumidor_b(ID_E_AUTOMATICO);
        buffer->consumidor_b(ID_E_DEFEITO);

        // valores "falsos" só para o 'cout' funcionar
        int pos_x = -1;
        int pos_y = -1;
        bool modo_auto = false;
        bool defeito = false;

        // --- 2. GERAR O LOG  ---
        string estado_str = (modo_auto ? "AUTOMATICO" : "MANUAL");
        string defeito_str = (defeito ? "COM_DEFEITO" : "NORMAL");

        cout << "[LOG] " << get_timestamp() 
             << " | CAMINHAO_0" << id_caminhao 
             << " | ESTADO: " << estado_str 
             << " | STATUS: " << defeito_str
             << " | POS: (" << pos_x << ", " << pos_y << ")" << endl;

        // Tarefa cíclica
        this_thread::sleep_for(chrono::seconds(2)); 
    }

    cout << "[Coletor Dados] Tarefa terminada." << endl;
}