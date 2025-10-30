#ifndef COLETOR_DADOS_H
#define COLETOR_DADOS_H

#include "buffer.h"
#include <atomic>

/**
 * @brief Executa a tarefa cíclica do Coletor de Dados.
 * * Conforme Etapa 1, esta tarefa consome dados de estado do
 * buffer (posição, modo de operação, falhas) e imprime um
 * log formatado no console.
 * * (Na Etapa 2, isso seria escrito em um arquivo de log).
 * * @param buffer Ponteiro para o buffer circular compartilhado.
 * @param running Referência para uma variável atômica que controla
 * a execução da thread.
 */
void tarefa_coletor_dados(Buffer_Circular* buffer, std::atomic<bool>& running);

#endif