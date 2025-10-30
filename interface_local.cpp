#include "interface_local.h"
#include "variaveis.h"
#include <iostream>
#include <thread>
#include <string>

using namespace std;

void tarefa_interface_local(Buffer_Circular* buffer, atomic<bool>& running) {
    
    cout << "[Interface Local] Tarefa iniciada. Aguardando comandos...." << endl;
    cout << "[Interface Local] Digite 'a' (Auto), 'm' (Manual), 'r' (Rearme) e pressione Enter:" << endl;
    
    char comando;
    
    // Este loop le entradas do usuário
    while (running && cin >> comando) {
        
        if (!running) break;

        switch(comando) {
            case 'a':
                // Produz o COMANDO para o modo automático
                buffer->produtor_b(true, ID_C_AUTOMATICO);
                cout << "[Interface Local] MODO AUTOMÁTICO enviado." << endl;
                break;
            case 'm':
                // Produz o COMANDO para o modo manual
                buffer->produtor_b(true, ID_C_MANUAL);
                cout << "[Interface Local] MODO MANUAL enviado." << endl;
                break;
            case 'r':
                // Produz o COMANDO de rearme
                buffer->produtor_b(true, ID_C_REARME);
                cout << "[Interface Local] REARME DE FALHA enviado." << endl;
                break;
            default:
                cout << "[Interface Local] Comando '" << comando << "' desconhecido." << endl;
        }
    }

    cout << "[Interface Local] Tarefa terminada." << endl;
}