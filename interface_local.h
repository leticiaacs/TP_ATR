#ifndef INTERFACE_LOCAL_H
#define INTERFACE_LOCAL_H

#include "buffer.h"
#include <atomic>
#include <mutex>

/**
 * @brief Executa a tarefa de Interface Local com o operador.
 *
 * - Lê comandos do teclado (a/m/r/w/s/d/e);
 * - Envia comandos para o coletor de dados via pipe
 *   ("automatico", "manual", "rearme", "acel_pos", "acel_neg", "dir_dir", "dir_esq");
 * - Coordena com a thread de exibição para não misturar prints enquanto o operador digita.
 *
 * @param buffer Ponteiro para o buffer circular compartilhado
 *        (não é usado diretamente aqui, mas mantido para compatibilidade de assinatura).
 * @param running Flag atômica para controle de execução.
 * @param leitor_coletor_dados Extremidade de escrita do pipe que envia comandos ao coletor.
 * @param leitor_interface_local Extremidade de leitura do pipe que recebe logs/estados do coletor.
 */
void tarefa_interface_local(Buffer_Circular* buffer,
                            std::atomic<bool>& running,
                            int &leitor_coletor_dados,
                            int &leitor_interface_local);

// ---------------------------------------------------------------------------
// Thread que lê comandos do operador (teclado) e manda para o coletor (pipe)
// Usa envio_cmd para sinalizar à thread de exibição que o operador está digitando.
// ---------------------------------------------------------------------------
void thread_recebe_cmd(Buffer_Circular* buffer,
                       std::atomic<bool>& running,
                       int &leitor_coletor_dados,
                       int &envio_cmd,
                       std::mutex &mtx_envio_cmd);

// ---------------------------------------------------------------------------
// Thread que exibe mensagens de estado vindas do coletor
// Só imprime quando envio_cmd == 0 (ou seja, ninguém está digitando comando).
// ---------------------------------------------------------------------------
void thread_exibe_msg(Buffer_Circular* buffer,
                      std::atomic<bool>& running,
                      int &leitor_interface_local,
                      int &envio_cmd,
                      std::mutex &mtx_envio_cmd);

#endif
