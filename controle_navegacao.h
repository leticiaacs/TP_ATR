#ifndef CONTROLE_NAVEGACAO_H
#define CONTROLE_NAVEGACAO_H

#include "buffer.h"
#include <atomic>

/**
 * @brief Tarefa responsável por controlar a navegação do caminhão.
 *
 * - Em modo AUTOMÁTICO:
 *      * Lê do buffer os setpoints de velocidade e ângulo (SP_VEL, SP_ANG_X);
 *      * Lê os estados atuais (posição, ângulo, modo automático);
 *      * Aplica controle PID em velocidade e ângulo;
 *      * Escreve no buffer os sinais de o_aceleracao e o_direcao.
 *
 * - Em modo MANUAL:
 *      * Zera a ação do controlador (PID);
 *      * Não interfere nos atuadores (quem manda é o operador).
 *
 * A periodicidade da tarefa é determinada pelos consumidores do Buffer_Circular
 * (sem uso de sleep), via semáforos internos do próprio buffer.
 */
void tarefa_controle_navegacao(Buffer_Circular* buffer, std::atomic<bool>& running);

#endif
