#include "controle_navegacao.h"
#include "variaveis.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

using namespace std;

void tarefa_controle_navegacao(Buffer_Circular* buffer, atomic<bool>& running) {

    cout << "[Controle Navegação] Tarefa iniciada (PID)." << endl;

    // --- Ganhos do controle PID ---
    const double Kp_vel = 0.8;
    const double Ki_vel = 0.2;
    const double Kd_vel = 0.1;

    const double Kp_ang = 1.0;
    const double Ki_ang = 0.3;
    const double Kd_ang = 0.2;

    // --- Variáveis de estado ---
    double setpoint_x = buffer->consumidor_i(ID_SP_POS_X);
    double setpoint_y = buffer->consumidor_i(ID_SP_POS_Y);
    double setpoint_ang = buffer->consumidor_i(ID_SP_ANG_X);

    double erro_vel_anterior = 0.0, erro_ang_anterior = 0.0;
    double integral_vel = 0.0, integral_ang = 0.0;

    bool automatico_anterior = false;

    // --- Parâmetros temporais ---
    const double dt = 0.2; // 200 ms = 0.2 s

    while (running) {

        // --- 1. Leitura dos sensores ---
        int pos_x = buffer->consumidor_i(ID_I_POS_X);
        int pos_y = buffer->consumidor_i(ID_I_POS_Y);
        int angulo = buffer->consumidor_i(ID_I_ANG_X);
//      int velocidade = buffer->consumidor_i(ID_I_VELOCIDADE);
        int velocidade = 60;   // n entendi da onde viria esse valor entao coloquei um valor ficticio 

        bool automatico = buffer->consumidor_b(ID_E_AUTOMATICO);

        // --- 2. Cálculo da velocidade de referência (distância até o setpoint) ---
        double dist = sqrt(pow(setpoint_x - pos_x, 2) + pow(setpoint_y - pos_y, 2));

        // --- 3. MODO AUTOMÁTICO ---
        if (automatico) {

            if (!automatico_anterior) {
                cout << "[Controle Navegação] Transição MANUAL → AUTOMÁTICO. (Bumpless transfer)" << endl;
            }

            // === CONTROLADOR DE VELOCIDADE (PID) ===
            double erro_vel = dist - velocidade; // queremos zerar a diferença entre velocidade alvo e atual
            integral_vel += erro_vel * dt;
            double derivada_vel = (erro_vel - erro_vel_anterior) / dt;

            double u_vel = Kp_vel * erro_vel + Ki_vel * integral_vel + Kd_vel * derivada_vel;
            erro_vel_anterior = erro_vel;

            // === CONTROLADOR ANGULAR (PID) ===
            double erro_ang = setpoint_ang - angulo;
            integral_ang += erro_ang * dt;
            double derivada_ang = (erro_ang - erro_ang_anterior) / dt;

            double u_ang = Kp_ang * erro_ang + Ki_ang * integral_ang + Kd_ang * derivada_ang;
            erro_ang_anterior = erro_ang;

            // --- Saturação ---
            if (u_vel > 100) u_vel = 100;
            if (u_vel < -100) u_vel = -100;
            if (u_ang > 180) u_ang = 180;
            if (u_ang < -180) u_ang = -180;

            // --- Escrita no buffer ---
            buffer->produtor_i(static_cast<int>(u_vel), ID_O_ACELERACAO);
            buffer->produtor_i(static_cast<int>(u_ang), ID_O_DIR);

            cout << "[Controle Navegação] AUTO | Vel=" << velocidade
                 << " spDist=" << dist << " -> Acel=" << u_vel
                 << " | Ang=" << angulo << " spAng=" << setpoint_ang
                 << " -> Dir=" << u_ang << endl;
        }
        // --- 4. MODO MANUAL ---
        else {

            if (automatico_anterior) {
                cout << "[Controle Navegação] Transição AUTOMÁTICO → MANUAL. (Bumpless transfer)" << endl;
            }

            // Atualiza setpoints com o estado atual (bumpless)
            setpoint_x = pos_x;
            setpoint_y = pos_y;
            setpoint_ang = angulo;

            // Zera termos integrais e derivativos (evita “windup”)
            integral_vel = 0.0;
            integral_ang = 0.0;
            erro_vel_anterior = 0.0;
            erro_ang_anterior = 0.0;

            // Desativa atuadores
            buffer->produtor_i(0, ID_O_ACELERACAO);
            buffer->produtor_i(0, ID_O_DIR);

            cout << "[Controle Navegação] MANUAL | Controle desligado. SP_x=" << setpoint_x
                 << " SP_y=" << setpoint_y << " SP_ang=" << setpoint_ang << endl;
        }

        automatico_anterior = automatico;
        this_thread::sleep_for(chrono::milliseconds(200)); // período de amostragem
    }

    cout << "[Controle Navegação] Tarefa encerrada." << endl;
}
