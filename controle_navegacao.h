#ifndef CONTROLE_NAVEGACAO_H
#define CONTROLE_NAVEGACAO_H

#include "buffer.h"
#include <atomic>

/**
 * @brief Tarefa responsável por controlar a navegação do caminhão.
 * - Em modo AUTOMÁTICO: calcula aceleração e direção com base nos setpoints.
 * - Em modo MANUAL: não interfere (os comandos vêm do operador).
 * - Escreve no buffer os valores de o_aceleracao e o_direcao.
 */
void tarefa_controle_navegacao(Buffer_Circular* buffer, std::atomic<bool>& running);

#endif
