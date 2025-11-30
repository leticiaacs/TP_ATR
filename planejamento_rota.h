#ifndef PLANEJAMENTO_ROTA_H
#define PLANEJAMENTO_ROTA_H

#include "buffer.h"
#include <atomic> // Para a flag de encerramento

/**
 * @brief Executa a tarefa cíclica de Planejamento de Rota.
 *
 * - Simula um planejamento de rota determinando, ao longo do tempo,
 *   setpoints de velocidade (SP_VEL) e ângulo (SP_ANG_X) para o caminhão;
 * - Escreve esses setpoints no Buffer_Circular para que o Controle de Navegação
 *   (tarefa_controle_navegacao) possa consumi-los;
 * - A periodicidade é controlada por um timer (ex.: Boost.Asio), sem uso de sleep.
 *
 * @param buffer Ponteiro para o buffer circular compartilhado.
 * @param running Flag atômica que controla a execução da tarefa
 *        (true = rodando, false = encerrar).
 */
void tarefa_planejamento_rota(Buffer_Circular* buffer, std::atomic<bool>& running, int id_caminhao);
#endif
