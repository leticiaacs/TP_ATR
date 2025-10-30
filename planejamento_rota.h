#ifndef PLANEJAMENTO_ROTA_H
#define PLANEJAMENTO_ROTA_H

#include "buffer.h"
#include <atomic> // Para a flag de encerramento

/**
 * @brief Executa a tarefa cíclica de Planejamento de Rota.
 * * Esta função simula o planejamento de rota, enviando setpoints
 * de posição (X, Y) para o buffer circular. Na Etapa 1, ela
 * simula uma rota pré-definida.
 * * @param buffer Ponteiro para o buffer circular compartilhado.
 * @param running Referência para uma variável atômica que controla
 * a execução da thread (true = rodando, false = parar).
 */
void tarefa_planejamento_rota(Buffer_Circular* buffer, std::atomic<bool>& running);

#endif