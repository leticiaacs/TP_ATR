#ifndef MONITORAMENTO_FALHAS_H
#define MONITORAMENTO_FALHAS_H

#include "buffer.h"
#include <atomic>

/**
 * @brief Tarefa responsável por monitorar falhas e condições críticas do caminhão.
 * - Lê sensores de temperatura e falhas elétricas/hidráulicas.
 * - Escreve no buffer o estado de defeito (e_defeito).
 */

void tarefa_monitoramento_falhas(Buffer_Circular* buffer, std::atomic<bool>& running);
void thread_falha_eletrica(Buffer_Circular* buffer, atomic<bool>& running, bool &falha_eletrica);
void thread_falha_hidraulica(Buffer_Circular* buffer, atomic<bool>& running, bool &falha_hidraulica);
void thread_temp(Buffer_Circular* buffer, atomic<bool>& running, bool &temp_defeito);
void thread_rearme(Buffer_Circular* buffer, atomic<bool>& running, bool &falha_eletrica, bool &falha_hidraulica, bool &temp_defeito);

#endif
