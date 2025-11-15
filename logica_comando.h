//função: setar atuadores o_aceleracao e o_direcao e setar estados e_defeito e e_automatico. 
// recebe diretamente informacoes do monitoramento de falhas para indicação de defeito e recebe via buffer informações
//de comandos para indicação de manual, automatico e comando de aceleracao e direcao.

#ifndef LOGICA_COMANDO_H
#define LOGICA_COMANDO_H
#define TEMPO_CICLO 1000

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <boost/asio.hpp>
#include "buffer.h"
#include "variaveis.h"
using namespace std;


class Logica_Comando{

private:

    int o_aceleracao = 0;
    int o_direcao = 0;
    bool e_defeito = false;
    bool e_automatico = false;
    bool c_rearme = false;

    void thread_info_defeito(Buffer_Circular& buffer);
    void info_defeito(Buffer_Circular& buffer);
    void thread_info_automatico(Buffer_Circular& buffer);
    void info_automatico(Buffer_Circular& buffer);
    void thread_saida_atuador_aceleracao(Buffer_Circular& buffer);
    int saida_atuador_aceleracao(Buffer_Circular& buffer);
    void thread_saida_atuador_direcao(Buffer_Circular& buffer);
    int saida_atuador_direcao(Buffer_Circular& buffer);
    void thread_rearme(Buffer_Circular& buffer);
    void rearme(Buffer_Circular& buffer);

public:

    void logica_comando(Buffer_Circular& buffer);

};

#endif