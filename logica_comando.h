// função: setar atuadores o_aceleracao e o_direcao e setar estados e_defeito e e_automatico.
// recebe diretamente informacoes do monitoramento de falhas para indicação de defeito e recebe via buffer informações
// de comandos para indicação de manual, automatico e comando de aceleracao e direcao.

#ifndef LOGICA_COMANDO_H
#define LOGICA_COMANDO_H

#define TEMPO_CICLO 1000  // em ms (usado pelos timers do Boost.Asio)

#include <thread>
#include <mutex>
#include <functional>
#include <atomic>
#include <boost/asio.hpp>
#include <iostream>
extern "C" {
    #include <mosquitto.h>
}
#include <string>
#include "buffer.h"
#include "variaveis.h"

class Logica_Comando {

private:
    // Sinais de saída para os atuadores (lado "C++")
    int  o_aceleracao = 0;
    int  o_direcao    = 0;

    // Estados publicados no buffer
    bool e_defeito     = false;
    bool e_automatico  = false;

    int esq_local;
    int dir_local;

    // Comando de rearme (lido do buffer e repassado ao monitoramento)
    bool c_rearme      = false;

    bool manual = true;
    bool automatico = false;
    int esq = 0;
    int dir = 0;

    mutex mtx_man;
    mutex mtx_auto;
    mutex mtx_esq;
    mutex mtx_dir;
    mutex mtx_acel;
    mutex mtx_direcao;

    // Tarefas periódicas internas (cada uma roda em sua própria thread)
    void thread_info_defeito(Buffer_Circular& buffer);
    void info_defeito(Buffer_Circular& buffer);

    void thread_info_automatico(Buffer_Circular& buffer);
    void info_automatico(Buffer_Circular& buffer);

    void thread_automatico_consome(Buffer_Circular& buffer);
    void automatico_consome(Buffer_Circular& buffer);

    void thread_manual_consome(Buffer_Circular& buffer);
    void manual_consome(Buffer_Circular& buffer);

    void thread_saida_atuador_aceleracao(Buffer_Circular& buffer);
    void  saida_atuador_aceleracao(Buffer_Circular& buffer);

    void thread_direita_consome(Buffer_Circular& buffer);
    void  direita_consome(Buffer_Circular& buffer);

    void thread_esquerda_consome(Buffer_Circular& buffer);
    void  esquerda_consome(Buffer_Circular& buffer);

    void thread_saida_atuador_direcao(Buffer_Circular& buffer);
    void  saida_atuador_direcao(Buffer_Circular& buffer);

    void thread_rearme(Buffer_Circular& buffer);
    void rearme(Buffer_Circular& buffer);

     // --- MQTT ---
    struct mosquitto* mqtt_client = nullptr;
    std::string mqtt_client_id;

    std::string topic_atuadores; // atr/caminhao/<id>/atuadores

    void mqtt_init(const std::string& broker, int port, int cam_id);
    void mqtt_loop(); // roda em thread separada

    static void mqtt_on_message_static(struct mosquitto* mosq,
                                       void* userdata,
                                       const struct mosquitto_message* msg);
    void mqtt_on_message(const struct mosquitto_message* msg);

    void logica_comando(Buffer_Circular& buffer);

public:
    /**
     * @brief Inicia todas as threads da Lógica de Comando.
     *
     * Responsabilidades:
     *  - Ler comandos do buffer (c_automatico, c_manual, c_acelera, c_direita, c_esquerda, c_rearme);
     *  - Ler flags globais de falha (defeito_var_global);
     *  - Publicar estados e_defeito e e_automatico no buffer;
     *  - Atualizar o_aceleracao e o_direcao para envio aos atuadores;
     *  - Repassar o comando de rearme para o monitoramento de falhas (rearme_var_global).
     */
    void start(Buffer_Circular& buffer,
               const std::string& broker,
               int port,
               int cam_id);
};

#endif
