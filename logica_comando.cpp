//função: setar atuadores o_aceleracao e o_direcao e setar estados e_defeito e e_automatico. 
// recebe diretamente informacoes do monitoramento de falhas para indicação de defeito e recebe via buffer informações
//de comandos para indicação de manual, automatico e comando de aceleracao e direcao.

#include "logica_comando.h"
#include "buffer.h"

void Logica_Comando::logica_comando(Buffer_Circular& buffer, bool falha){

    // Threads para cada sensor
    thread t_def(&Logica_Comando::thread_info_defeito, this, ref(buffer), falha); //uso do ref para passar buffer como referência e nao cópia
    thread t_auto(&Logica_Comando::thread_info_automatico, this, ref(buffer));// uso do this porque o método é um método da própria clase
    thread t_acelera(&Logica_Comando::thread_saida_atuador_aceleracao, this, ref(buffer));
    thread t_direcao(&Logica_Comando::thread_saida_atuador_direcao, this, ref(buffer));
    
    // Espera todas terminarem
    t_def.join();
    t_auto.join();
    t_acelera.join();
    t_direcao.join();

}

void Logica_Comando::thread_info_defeito(Buffer_Circular& buffer, bool falha){

    //THREAD PERIÓDICA
    if(falha == true){
        this->e_defeito = true;
    } else {
        this->e_defeito = false;
    }

    buffer.produtor_b(this->e_defeito, ID_DEFEITO_E);
}

void Logica_Comando::thread_info_automatico(Buffer_Circular& buffer){

    //THREAD PERIÓDICA

    bool comando_auto;
    bool comando_man;
    comando_auto = buffer.consumidor_b(ID_AUTO_C);
    comando_man = buffer.consumidor_b(ID_AUTO_C);

    if(comando_auto == false && comando_man == true){
        this->e_automatico = false;
    } else if (comando_auto == true && comando_man == false){
       this->e_automatico = true;
    }

    buffer.produtor_b(this->e_automatico, ID_AUTO_E);

}

int  Logica_Comando::thread_saida_atuador_aceleracao(Buffer_Circular& buffer){

    //THREAD PERIÓDICA
    int aceleracao = buffer.consumidor_i(ID_ACELERA_C);
    return this->o_aceleracao;
    //retorno será do tipo publisher/subscriber, adequar
}

int  Logica_Comando::thread_saida_atuador_direcao(Buffer_Circular& buffer){

    //THREAD PERIÓDICA
    int soma = 0;
    int subtracao = 0;

    soma = buffer.consumidor_i(ID_ESQUERDA_C);
    subtracao = buffer.consumidor_i(ID_DIREITA_C);

    o_direcao = soma - subtracao;

    return this->o_direcao;
    //retorno será do tipo publisher/subscriber, adequar
}