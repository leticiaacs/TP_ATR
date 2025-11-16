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
void tarefa_coletor_dados(Buffer_Circular* buffer, std::atomic<bool>& running, int id_caminhao, int &leitor_coletor_dados, int &leitor_interface_local);
void thread_pos_x(Buffer_Circular* buffer, atomic<bool>& running, float &pos_x);
void thread_pos_y(Buffer_Circular* buffer, atomic<bool>& running, float &pos_y);
void thread_modo_auto(Buffer_Circular* buffer, atomic<bool>& running, bool &modo_auto);
void thread_defeito(Buffer_Circular* buffer, atomic<bool>& running, bool &defeito);
void thread_armazena(Buffer_Circular* buffer, atomic<bool>& running, int id_caminhao, float &pos_x, float &pos_y, bool &modo_auto, bool &defeito, int &leitor_coletor_dados, int &leitor_interface_local);
void thread_envia_buffer(Buffer_Circular* buffer, atomic<bool>& running, int &leitor_coletor_dados, int &leitor_interface_local, bool &automatico, int &contador_rearme);
void thread_recebe_pipe(Buffer_Circular* buffer, atomic<bool>& running, int &leitor_coletor_dados, bool &automatico, int &contador_rearme);   

#endif