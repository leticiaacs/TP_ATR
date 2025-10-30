#include "planejamento_rota.h"
#include "variaveis.h" // Para ter os IDs das variáveis
#include <iostream>
#include <thread>
#include <chrono> // Para o "sleep_for"
#include <vector>

using namespace std;

// Para a Etapa 1, vamos simular uma rota com alguns pontos
struct Waypoint {
    int x;
    int y;
};

// Rota simulada (Quadradi)
vector<Waypoint> rota_simulada = {
    {100, 0},
    {100, 100},
    {0, 100},
    {0, 0}
};

int ind_atual = 0; // Índice atual


void tarefa_planejamento_rota(Buffer_Circular* buffer, atomic<bool>& running) {
    
    cout << "[Planejamento de Rota] Tarefa iniciada." << endl;

    // Loop principal da tarefa
    while (running) {
        
        Waypoint sp_atual = rota_simulada[ind_atual];
        
        // Escreve o setpoint 
        buffer->produtor_i(sp_atual.x, ID_SP_POS_X); 
        buffer->produtor_i(sp_atual.y, ID_SP_POS_Y);

        ind_atual = (ind_atual + 1) % rota_simulada.size();
        this_thread::sleep_for(chrono::seconds(2)); 
    }
    cout << "[Planejamento de Rota] Tarefa terminada." << endl;
}