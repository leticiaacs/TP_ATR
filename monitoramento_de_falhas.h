#ifndef MONITORAMENTO_FALHAS_H
#define MONITORAMENTO_FALHAS_H

#include "buffer.h"
#include <atomic>
#include <string>

/**
 * @brief Tarefa responsável por monitorar falhas e condições críticas do caminhão.
 *
 * Agora:
 *  - Lê falhas e temperatura diretamente do MQTT:
 *      tópico: atr/caminhao/<id>/falhas
 *      payload: { "id", "temp", "falha_eletrica", "falha_hidraulica" }
 *  - Atualiza variáveis globais de estado (defeito_var_global, falha_*_global).
 *  - A Lógica de Comando usa essas flags para gerar e_defeito no buffer.
 *
 * @param buffer       Ponteiro para o buffer (não é usado diretamente aqui, mas mantido
 *                     para compatibilidade de interface).
 * @param running      Flag de execução (true = roda; false = encerra).
 * @param caminhao_id  ID do caminhão (usado para montar o tópico MQTT de falhas).
 * @param broker       Endereço do broker MQTT (default "localhost").
 * @param broker_port  Porta do broker MQTT (default 1883).
 */
void tarefa_monitoramento_falhas(Buffer_Circular* buffer,
                                 std::atomic<bool>& running,
                                 int caminhao_id,
                                 const std::string& broker = "localhost",
                                 int broker_port = 1883);

#endif
