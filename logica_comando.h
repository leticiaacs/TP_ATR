//função: setar atuadores o_aceleracao e o_direcao e setar estados e_defeito e e_automatico. 
// recebe diretamente informacoes do monitoramento de falhas para indicação de defeito e recebe via buffer informações
//de comandos para indicação de manual, automatico e comando de aceleracao e direcao.

#ifndef LOGICA_COMANDO_H
#define LOGICA_COMANDO_H

#include <iostream>
#include <vector>
#include <thread>
using namespace std;

#define ID_ACELERA_C 6
#define ID_DIREITA_C 7
#define ID_ESQUERDA_C 8
#define ID_DEFEITO_E 11
#define ID_AUTO_E 12
#define ID_AUTO_C 13
#define ID_MANUAL_C 14
#define ID_ACELERACAO_O 4
#define ID_DIRECAO_O 5

class Logica_Comando{

private:

    int o_aceleracao = 0;
    int o_direcao = 0;
    bool e_defeito = false;
    bool e_automatico = false;

    void thread_info_defeito(Buffer_Circular& buffer, bool falha);
    void thread_info_automatico(Buffer_Circular& buffer);
    int thread_saida_atuador_aceleracao(Buffer_Circular& buffer);
    int thread_saida_atuador_direcao(Buffer_Circular& buffer);

public:

    void logica_comando(Buffer_Circular& buffer, bool falha);

};

#endif