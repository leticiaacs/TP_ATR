#include "controle_navegacao.h"
#include "variaveis.h"

#include <iostream>
#include <cmath>
#include <functional>
#include <boost/asio.hpp>

using namespace std;

void tarefa_controle_navegacao(Buffer_Circular* buffer,
                               std::atomic<bool>& running)
{
    cout << "[Controle Navegação] Tarefa iniciada (PID)." << endl;

    // --- Ganhos PID (ajustáveis) ---
    const double Kp_vel = 0.8;
    const double Ki_vel = 0.2;
    const double Kd_vel = 0.1;

    const double Kp_ang = 1.0;
    const double Ki_ang = 0.3;
    const double Kd_ang = 0.2;

    // --- Estado interno do controlador ---
    const double dt = 1.0; // 1 s (mesmo período do planejamento de rota)

    double erro_vel_anterior = 0.0;
    double erro_ang_anterior = 0.0;
    double integral_vel      = 0.0;
    double integral_ang      = 0.0;

    bool automatico_anterior = false;

    double ult_pos_x = 0.0;
    double ult_pos_y = 0.0;
    bool   tem_pos_ant = false;

    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(1000));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code& ec) {
        if (ec) return;
        if (!running) return;

        // -----------------------------------------------------------------
        // 1) Leitura dos estados filtrados do buffer (sensores)
        //    ID da tarefa consumidor: 2 (controle_navegacao)
        // -----------------------------------------------------------------
        double pos_x  = buffer->consumidor_i(ID_I_POS_X, 2);
        double pos_y  = buffer->consumidor_i(ID_I_POS_Y, 2);
        double angulo = buffer->consumidor_i(ID_I_ANG_X, 2);
        bool automatico = buffer->consumidor_b(ID_E_AUTOMATICO, 2);

        // -----------------------------------------------------------------
        // 2) Leitura dos setpoints de velocidade e ângulo
        // -----------------------------------------------------------------
        double sp_vel = buffer->consumidor_i(ID_SP_VEL,   2);
        double sp_ang = buffer->consumidor_i(ID_SP_ANG_X, 2);

        // Estimativa de velocidade a partir da diferença de posição
        double vel_atual = 0.0;
        if (tem_pos_ant) {
            double dx = pos_x - ult_pos_x;
            double dy = pos_y - ult_pos_y;
            vel_atual = std::sqrt(dx*dx + dy*dy) / dt;
        }
        ult_pos_x   = pos_x;
        ult_pos_y   = pos_y;
        tem_pos_ant = true;

        if (automatico) {
            // ----------------------------- AUTOMÁTICO -----------------------------
            if (!automatico_anterior) {
                cout << "[Controle Navegação] MANUAL → AUTOMÁTICO (bumpless)." << endl;
            }

            // PID de velocidade
            double erro_vel = sp_vel - vel_atual;
            integral_vel += erro_vel * dt;
            double derivada_vel = (erro_vel - erro_vel_anterior) / dt;
            double u_vel = Kp_vel*erro_vel + Ki_vel*integral_vel + Kd_vel*derivada_vel;
            erro_vel_anterior = erro_vel;

            // PID angular
            double erro_ang = sp_ang - angulo;
            integral_ang += erro_ang * dt;
            double derivada_ang = (erro_ang - erro_ang_anterior) / dt;
            double u_ang = Kp_ang*erro_ang + Ki_ang*integral_ang + Kd_ang*derivada_ang;
            erro_ang_anterior = erro_ang;

            // Saturações
            if (u_vel > 100)  u_vel = 100;
            if (u_vel < -100) u_vel = -100;
            if (u_ang > 180)  u_ang = 180;
            if (u_ang < -180) u_ang = -180;

            int cmd_acel = static_cast<int>(std::round(u_vel));
            int cmd_ang  = static_cast<int>(std::round(u_ang));

            // Converte comando angular em "direita" / "esquerda"
            int cmd_esq = (cmd_ang > 0) ? cmd_ang : 0;
            int cmd_dir = (cmd_ang < 0) ? -cmd_ang : 0;

            // *** ALTERADO: comandos vão para C_ACELERA / C_DIREITA / C_ESQUERDA ***
            buffer->produtor_i(cmd_acel, ID_C_ACELERA);
            buffer->produtor_i(cmd_dir,  ID_C_DIREITA);
            buffer->produtor_i(cmd_esq,  ID_C_ESQUERDA);

            cout << "[Controle Navegação] AUTO | vel_atual=" << vel_atual
                 << " sp_vel=" << sp_vel  << " cmd_acel=" << cmd_acel
                 << " | ang=" << angulo  << " sp_ang=" << sp_ang
                 << " cmd_ang=" << cmd_ang << endl;
        }
        else {
            // ----------------------------- MANUAL -----------------------------
            if (automatico_anterior) {
                cout << "[Controle Navegação] AUTOMÁTICO → MANUAL (controle desligado)." << endl;
            }

            // "Bumpless transfer": zera integrais e derivações
            integral_vel = integral_ang = 0.0;
            erro_vel_anterior = erro_ang_anterior = 0.0;

            // Em modo manual, quem manda é o operador (via Interface Local),
            // então zeramos os comandos automáticos.
            buffer->produtor_i(0, ID_C_ACELERA);
            buffer->produtor_i(0, ID_C_DIREITA);
            buffer->produtor_i(0, ID_C_ESQUERDA);

            cout << "[Controle Navegação] MANUAL | Controle PID desligado." << endl;
        }

        automatico_anterior = automatico;

        // Reprograma o próximo disparo (tarefa periódica sem sleep)
        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(1000));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();

    cout << "[Controle Navegação] Tarefa encerrada." << endl;
}
