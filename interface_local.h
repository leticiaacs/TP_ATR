#ifndef INTERFACE_LOCAL_H
#define INTERFACE_LOCAL_H

#include "buffer.h"
#include <atomic>

/**
 * @brief Executa a tarefa de Interface Local com o operador.
 * * Esta tarefa roda em sua própria thread, lendo comandos do
 * teclado (e.g., 'a', 'm') e produzindo-os no buffer
 * para a Lógica de Comando consumir.
 * * @param buffer Ponteiro para o buffer circular compartilhado.
 * @param running Referência para uma variável atômica que controla
 * a execução da thread.
 */
void tarefa_interface_local(Buffer_Circular* buffer, std::atomic<bool>& running);

#endif