//função: setar atuadores o_aceleracao e o_direcao e setar estados e_defeito e e_automatico. 
// recebe diretamente informacoes do monitoramento de falhas para indicação de defeito e recebe via buffer informações
//de comandos para indicação de manual, automatico e comando de aceleracao e direcao.

#include "logica_comando.h"
#include "monitoramento_de_falhas.h"
#include "coletor_dados.h"


void Logica_Comando::logica_comando(Buffer_Circular& buffer){

    // Threads para cada sensor
    thread t_def(&Logica_Comando::thread_info_defeito, this, ref(buffer)); //uso do ref para passar buffer como referência e nao cópia
    thread t_auto(&Logica_Comando::thread_info_automatico, this, ref(buffer));// uso do this porque o método é um método da própria classe
    thread t_acelera(&Logica_Comando::thread_saida_atuador_aceleracao, this, ref(buffer));
    thread t_direcao(&Logica_Comando::thread_saida_atuador_direcao, this, ref(buffer));
    thread t_rearme(&Logica_Comando::thread_rearme, this, ref(buffer));
    
    // Espera todas terminarem
    t_def.join();
    t_auto.join();
    t_acelera.join();
    t_direcao.join();
    t_rearme.join();

}

void Logica_Comando::thread_info_defeito(Buffer_Circular& buffer){
    //falha será enviado pelo monitoramento de falhas - apenas fique ciente de que cada 
   // thread irá rodar indefinidamente e que qualquer alteração no valor de falha precisaria ser feita
    // com variável atômica ou referência para refletir dentro da thread.

    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code&) {
        this->info_defeito(buffer);

        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();

}

void Logica_Comando::info_defeito(Buffer_Circular& buffer){
    mtx_defeito.lock();
    if(defeito_var_global == true){
        mtx_defeito.unlock();
        this->e_defeito = true;
    } else {
        mtx_defeito.unlock();
        this->e_defeito = false;
    }

    buffer.produtor_b(this->e_defeito, ID_E_DEFEITO);

}


void Logica_Comando::thread_info_automatico(Buffer_Circular& buffer){


    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code&) {
        this->info_automatico(buffer);


        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();

}

void Logica_Comando::info_automatico(Buffer_Circular& buffer){

    //THREAD PERIÓDICA

    bool comando_auto;
    bool comando_man;
    comando_auto = buffer.consumidor_b(ID_C_AUTOMATICO, 0);
    comando_man = buffer.consumidor_b(ID_C_MANUAL, 0);

    mtx_defeito.lock();
    if((comando_auto == false && comando_man == true)||(defeito_var_global == true)){
        this->e_automatico = false;
    } else if (comando_auto == true && comando_man == false){
       this->e_automatico = true;
    }
    mtx_defeito.unlock();

    buffer.produtor_b(this->e_automatico, ID_E_AUTOMATICO);

}

void  Logica_Comando::thread_saida_atuador_aceleracao(Buffer_Circular& buffer){

    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code&) {
        this->saida_atuador_aceleracao(buffer);

        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();

}

int  Logica_Comando::saida_atuador_aceleracao(Buffer_Circular& buffer){

    //THREAD PERIÓDICA
    int aceleracao = buffer.consumidor_i(ID_C_ACELERA, 0);
    return this->o_aceleracao;
    //retorno será do tipo publisher/subscriber, adequar para enviar para atuadores
}

void  Logica_Comando::thread_saida_atuador_direcao(Buffer_Circular& buffer){

    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code&) {
        this->saida_atuador_direcao(buffer);

        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();

}

int  Logica_Comando::saida_atuador_direcao(Buffer_Circular& buffer){

    int soma = 0;
    int subtracao = 0;

    soma = buffer.consumidor_i(ID_C_ESQUERDA, 0);
    subtracao = buffer.consumidor_i(ID_C_DIREITA, 0);

    o_direcao = soma - subtracao;

    return this->o_direcao;
    //retorno será do tipo publisher/subscriber, adequar para enviar para atuadores
}

void Logica_Comando::thread_rearme(Buffer_Circular& buffer){
    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(100));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code&) {
        this->rearme(buffer);

        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(100));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();

}

void Logica_Comando::rearme(Buffer_Circular& buffer){
    c_rearme = buffer.consumidor_b(12, 0);
    mtx_rearme.lock();
    rearme_var_global = c_rearme;
    mtx_rearme.unlock();

}

int main(){
    Buffer_Circular buffer;
    Logica_Comando logica;
    std::atomic<bool> running(true);

    // Thread 1 - Monitoramento de falhas
    std::thread t_monitor([&]() {
        tarefa_monitoramento_falhas(&buffer, running);
    });

    // Thread 2 - Lógica de comando
    std::thread t_logica([&]() {
        logica.logica_comando(buffer);
    });

    std::thread t_coletor([&]() {
        tarefa_coletor_dados(&buffer, running, 1);
    });

    // -------------------------------
    // Simula o sistema rodando por 10 segundos
    // -------------------------------
    for (int i = 0; i < 70; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if ((i == 1)||(i == 2)||(i == 3)||(i == 4) ){
            buffer.produtor_b(true,10);
            buffer.produtor_b(false,12);
            buffer.consumidor_b(8,0);

        } else if ((i == 13)||(i == 14)||(i == 15)) {
            buffer.produtor_b(true,12);
            buffer.produtor_b(false,10);
            buffer.consumidor_b(8,0);

        } else {
            buffer.produtor_b(false,10);
            buffer.produtor_b(false,12);
            buffer.consumidor_b(8,0);
        }
        buffer.mostrar_buffer();
    std::cout << defeito_var_global << std::endl;
        std::cout << rearme_var_global << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // -------------------------------
    // Encerramento
    // -------------------------------
    running = false;  
    std::this_thread::sleep_for(std::chrono::seconds(1)); 

    std::cout << "\n[Main] Encerrando threads..." << std::endl;

    if (t_monitor.joinable()) t_monitor.join();
    if (t_logica.joinable()) t_logica.join();
    if (t_coletor.joinable()) t_coletor.join(); // ⭐ nova thread sendo fechada

    std::cout << "[Main] Fim da simulação." << std::endl;
    return 0;
}