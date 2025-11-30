#ifndef coletor_dados_h
#define coletor_dados_h

#include "buffer.h"
#include <atomic>
#include <mutex>

/**
 * 1. PRODUTOR: Envia comandos (Auto/Manual) para o Buffer 
 * 2. CONSUMIDOR: Lê estados (Posição/Defeitos) do Buffer 
*/

void tarefa_coletor_dados(Buffer_Circular* buffer, std::atomic<bool>& running, int id_caminhao, int &leitor_coletor_dados, int &leitor_interface_local);

// Thread única para ler todos os sensores de uma vez 
void thread_leitura_sensor_posx(Buffer_Circular* buffer, std::atomic<bool>& running, float &pos_x);

void thread_leitura_sensor_posy(Buffer_Circular* buffer, std::atomic<bool>& running, float &pos_y);

void thread_leitura_sensor_ang(Buffer_Circular* buffer, std::atomic<bool>& running, float &angulo);

void thread_leitura_sensor_auto(Buffer_Circular* buffer, std::atomic<bool>& running, bool &modo_auto);

void thread_leitura_sensor_posx(Buffer_Circular* buffer, std::atomic<bool>& running, bool &defeito);

// Thread para armazenar/logar os dados lidos 
void thread_armazena(std::atomic<bool>& running, int id_caminhao, float &pos_x, float &pos_y, float &angulo, bool &modo_auto, bool &defeito, int &leitor_interface_local);

// Thread PRODUTORA: Envia comandos para o buffer continuamente
void thread_envia_buffer(Buffer_Circular* buffer, std::atomic<bool>& running, bool &automatico, int &contador_rearme, int &cmd_acel, int &cmd_dir, bool &cmd_acel_pendente, bool &cmd_dir_pendente);

// Thread de INTERFACE: Recebe inputs do usuário (pipe) e atualiza variáveis de comando
void thread_recebe_pipe(std::atomic<bool>& running, int &leitor_coletor_dados, bool &automatico, int &contador_rearme, int &cmd_acel, int &cmd_dir, bool &cmd_acel_pendente, bool &cmd_dir_pendente);


#endif