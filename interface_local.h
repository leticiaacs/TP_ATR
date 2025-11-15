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
void tarefa_interface_local(Buffer_Circular* buffer, std::atomic<bool>& running, int leitor_coletor_dados, int leitor_interface_local);
void thread_recebe_cmd(Buffer_Circular* buffer, std::atomic<bool>& running, int leitor_coletor_dados, int &envio_cmd,  mutex &mtx_envio_cmd);
void thread_exibe_msg(Buffer_Circular* buffer, std::atomic<bool>& running, int leitor_interface_local, int &envio_cmd,  mutex &mtx_envio_cmd);


#endif