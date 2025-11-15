#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>   // Para a flag de controle
#include <csignal>  // Para capturar o Ctrl+C (SIGINT)

#include "buffer.h"
#include "variaveis.h"
#include "planejamento_rota.h"
#include "coletor_dados.h"
#include "interface_local.h" 
#include "controle_navegacao.h"
#include "monitoramento_de_falhas.h" 

using namespace std;

// --- Variáveis Globais de Controle ---
Buffer_Circular buffer_compartilhado;
bool defeito_var_global = false;
bool rearme_var_global = false;
atomic<bool> running(true);

/**
 * @brief Função para tratar o sinal SIGINT (Ctrl+C).
 */
void signal_handler(int signum) {
    if (signum == SIGINT) {
        cout << "\n[Main] Sinal (Ctrl+C) recebido. Encerrando as tarefas..." << endl;
        running = false; // Sinaliza para as threads pararem
    }
}

// --- STUBS (SIMULAÇÕES) ---

/**
 * @brief STUB (Simulação) da tarefa Tratamento Sensores.
 * * Produz posições X e Y para destravar o Coletor de Dados.
 */
void tarefa_stub_sensores(Buffer_Circular* buffer, atomic<bool>& running) {
    cout << "[Stub Sensores] Tarefa iniciada. (Simulando posições)" << endl;
    int pos_x = 0;
    int pos_y = 0;
    while(running) {
        pos_x += 1; 
        pos_y += 1;
        buffer->produtor_i(pos_x, ID_I_POS_X);
        buffer->produtor_i(pos_y, ID_I_POS_Y);
        this_thread::sleep_for(chrono::milliseconds(500));
    }
    cout << "[Stub Sensores] Tarefa terminada." << endl;
}

/**
 * @brief STUB (Simulação) da tarefa Lógica de Comando.
 * * ESTA VERSÃO É INTERATIVA!
 * * Consome os comandos da Interface Local (ID_C_*)
 * * Produz o estado atual do caminhão (ID_E_*)
 */
void tarefa_stub_logica_comando(Buffer_Circular* buffer, atomic<bool>& running) {
    cout << "[Stub Lógica] Tarefa iniciada. (Aguardando comandos da Interface)" << endl;
    
    // Variáveis de estado interno do caminhão
    bool estado_auto = false; // Começa em manual
    bool estado_defeito = false; // Começa sem defeito

    while(running) {
        // --- CONSOME OS COMANDOS ---
        buffer->consumidor_b(ID_C_AUTOMATICO);
        buffer->consumidor_b(ID_C_MANUAL);
        buffer->consumidor_b(ID_C_REARME);
        
        // --- PRODUZ OS ESTADOS ATUAIS ---
        // O Coletor de Dados vai consumir isso
        buffer->produtor_b(estado_auto, ID_E_AUTOMATICO);
        buffer->produtor_b(estado_defeito, ID_E_DEFEITO);
        
        this_thread::sleep_for(chrono::seconds(1));
    }
    cout << "[Stub Lógica] Tarefa terminada." << endl;
}


// --- Função Principal ---

int main() {
    signal(SIGINT, signal_handler);

    cout << "[Main] Iniciando sistema de controle do caminhão..." << endl;
    cout << "[Main] Pressione (Ctrl+C) para encerrar." << endl;
    
    // Tarefa de Planejamento de Rota
    thread t_planejamento(tarefa_planejamento_rota, 
                          &buffer_compartilhado,
                          ref(running)); 
                          
    // STUB de Sensores (Produtor de Posição)
    thread t_stub_sensores(tarefa_stub_sensores, 
                           &buffer_compartilhado, 
                           ref(running));

    // STUB da Lógica de Comando (Consome Comandos, Produz Estado)
    thread t_stub_logica(tarefa_stub_logica_comando, 
                         &buffer_compartilhado, 
                         ref(running));
                         
    //Tarefa Coletor de Dados (Consome Estado, Imprime Log)
    thread t_coletor(tarefa_coletor_dados, 
                     &buffer_compartilhado, 
                     ref(running));
                     
    // Tarefa Interface Local (Produz Comandos do Teclado)
    thread t_interface(tarefa_interface_local, 
                       &buffer_compartilhado, 
                       ref(running));
    
    // Tarefa Controle de Navegação
    thread t_nav(tarefa_controle_navegacao, 
                        &buffer_compartilhado,
                        ref(running));

    // Tarefa Monitoramento de Falhas
    thread t_falhas(tarefa_monitoramento_falhas,
                        &buffer_compartilhado,
                        ref(running));
                     
    // --- Aguarda o Ctrl+C ---
    while (running) {
        this_thread::sleep_for(chrono::milliseconds(100));
    }


    cout << "[Main] Aguardando threads finalizarem..." << endl;

    if (t_planejamento.joinable()) {
        t_planejamento.join();
    }
    if (t_stub_sensores.joinable()) {
        t_stub_sensores.join();
    }
    if (t_stub_logica.joinable()) {
        t_stub_logica.join();
    }
    if (t_coletor.joinable()) {
        t_coletor.join();
    }
    if (t_interface.joinable()) { 
        t_interface.join();
    }
    if (t_nav.joinable()){
        t_nav.join();
    }
    if (t_falhas.joinable()){
        t_falhas.join();
    }
    cout << "[Main] Sistema encerrado com sucesso." << endl;
    return 0;
}