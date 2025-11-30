// função: setar atuadores o_aceleracao e o_direcao e setar estados e_defeito e e_automatico.
// recebe diretamente informacoes do monitoramento de falhas para indicação de defeito e
// recebe via buffer informações de comandos para indicação de manual, automatico e
// comando de aceleracao e direcao.

#include "logica_comando.h"
#include "monitoramento_de_falhas.h"
//#include "coletor_dados.h"

void Logica_Comando::mqtt_init(const std::string& broker, int port, int cam_id)
{
    mosquitto_lib_init();

    mqtt_client_id = "logica_cmd_" + std::to_string(cam_id);
    topic_atuadores = "atr/caminhao/" + std::to_string(cam_id) + "/atuadores";

    mqtt_client = mosquitto_new(mqtt_client_id.c_str(), true, this);
    if (!mqtt_client) {
        throw std::runtime_error("falha mosquitto_new");
    }

    mosquitto_connect(mqtt_client, broker.c_str(), port, 60);
}

void Logica_Comando::mqtt_loop()
{
    // loop simples bloqueante
    while (true) {
        int rc = mosquitto_loop(mqtt_client, -1, 1);
        if (rc != MOSQ_ERR_SUCCESS) {
            std::cerr << "[logica] erro mosquitto_loop: " << rc << std::endl;
            break;
        }

        // a cada ciclo, publica atuadores/estados no tópico "atuadores"
        // (usa os membros o_aceleracao, o_direcao, e_automatico, e_defeito)
        int acel, dir;
        bool auto_flag, def_flag;

        {
            // travar o que for necessário (ex.: mtx_auto, mtx_defeito, etc.)
            std::lock_guard<std::mutex> lock_a(mtx_auto);
            auto_flag = e_automatico;
            std::lock_guard<std::mutex> lock_d(mtx_defeito);
            def_flag  = e_defeito;
            std::lock_guard<std::mutex> lock_d(mtx_acel);
            acel = o_aceleracao;
            std::lock_guard<std::mutex> lock_d(mtx_direcao);
            dir = o_direcao;
        }

        std::string payload = "{"
            "\"acel\":"       + std::to_string(acel) +
            ",\"dir\":"       + std::to_string(dir) +
            ",\"automatico\":" + std::string(auto_flag ? "true" : "false") +
            ",\"defeito\":"    + std::string(def_flag ? "true" : "false") +
            "}";

        mosquitto_publish(mqtt_client, nullptr,
                          topic_atuadores.c_str(),
                          payload.size(), payload.c_str(),
                          0, false);
    }

    mosquitto_destroy(mqtt_client);
    mosquitto_lib_cleanup();
}


void Logica_Comando::start(Buffer_Circular& buffer,
                           const std::string& broker,
                           int port,
                           int cam_id)
{
    // configura tópicos
    mqtt_init(broker, port, cam_id);

    // thread do MQTT
    std::thread t_mqtt(&Logica_Comando::mqtt_loop, this);

    // threads internas da lógica de comando (já existentes)
    logica_comando(buffer);   // esta função já cria as threads "t_def", "t_auto", etc. :contentReference[oaicite:7]{index=7}

    t_mqtt.join();
}

void Logica_Comando::logica_comando(Buffer_Circular& buffer) {

    // Threads para cada “linha” de lógica
    thread t_def(&Logica_Comando::thread_info_defeito,    this, ref(buffer));
    thread t_auto(&Logica_Comando::thread_info_automatico,this, ref(buffer));
    thread t_auto_c(&Logica_Comando::thread_automatico_consome,this, ref(buffer));
    thread t_man_c(&Logica_Comando::thread_manual_consome,this, ref(buffer));
    thread t_acel(&Logica_Comando::thread_saida_atuador_aceleracao, this, ref(buffer));
    thread t_esq_c(&Logica_Comando::thread_esquerda_consome,this, ref(buffer));
    thread t_dir_c(&Logica_Comando::thread_direita_consome,this, ref(buffer));
    thread t_dir (&Logica_Comando::thread_saida_atuador_direcao,    this, ref(buffer));
    thread t_rea (&Logica_Comando::thread_rearme,         this, ref(buffer));

    t_def.join();
    t_auto.join();
    t_auto_c.join();
    t_man_c.join();
    t_acel.join();
    t_dir_c.join();
    t_esq_c.join();
    t_dir.join();
    t_rea.join();
}

// -----------------------------------------------------------------------------
// e_defeito: lido direto de defeito_var_global (monitoramento_de_falhas)
// e enviado para o buffer em ID_E_DEFEITO
// -----------------------------------------------------------------------------
void Logica_Comando::thread_info_defeito(Buffer_Circular& buffer) {

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

void Logica_Comando::info_defeito(Buffer_Circular& buffer) {

    {
        lock_guard<mutex> lock(mtx_defeito);
        e_defeito = defeito_var_global;
    }

    buffer.produtor_b(e_defeito, ID_E_DEFEITO);
}

void Logica_Comando::thread_automatico_consome(Buffer_Circular& buffer) {
    boost::asio::io_context io;
        boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

        std::function<void(const boost::system::error_code&)> handler;
        handler = [&](const boost::system::error_code&) {
            this->automatico_consome(buffer);

            timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
            timer.async_wait(handler);
        };

        timer.async_wait(handler);
        io.run();

}

void Logica_Comando::automatico_consome(Buffer_Circular& buffer) {
    {
        lock_guard<mutex> lock(mtx_auto);
        this->automatico = buffer.consumidor_b(ID_C_AUTOMATICO, 0);
    }
}

void Logica_Comando::thread_manual_consome(Buffer_Circular& buffer) {
    boost::asio::io_context io;
        boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

        std::function<void(const boost::system::error_code&)> handler;
        handler = [&](const boost::system::error_code&) {
            this->manual_consome(buffer);

            timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
            timer.async_wait(handler);
        };

        timer.async_wait(handler);
        io.run();

}

void Logica_Comando::manual_consome(Buffer_Circular& buffer) {
    {
        lock_guard<mutex> lock(mtx_man);
        this->manual = buffer.consumidor_b(ID_C_MANUAL, 0);
    }
}

// -----------------------------------------------------------------------------
// e_automatico: calculado a partir de c_automatico / c_man (buffer) + defeito
// -----------------------------------------------------------------------------
void Logica_Comando::thread_info_automatico(Buffer_Circular& buffer) {

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

void Logica_Comando::info_automatico(Buffer_Circular& buffer) {

    bool comando_auto;
    {
        lock_guard<mutex> lock(mtx_auto);
        comando_auto = this->automatico;
    }

    bool comando_man;
    {
        lock_guard<mutex> lock(mtx_man);
        comando_auto = this->manual;
    }

    bool tem_defeito;
    {
        lock_guard<mutex> lock(mtx_defeito);
        tem_defeito = defeito_var_global;
    }
    
    if ((comando_auto == false && comando_man == true) || tem_defeito) {
        e_automatico = false;
    } else if (comando_auto == true && comando_man == false) {
        e_automatico = true;
    }
    // (caso os dois venham true/false, mantemos estado anterior)

    buffer.produtor_b(e_automatico, ID_E_AUTOMATICO);
}

// -----------------------------------------------------------------------------
// Saída de atuador de aceleração: lê c_acelera do buffer
// -----------------------------------------------------------------------------
void Logica_Comando::thread_saida_atuador_aceleracao(Buffer_Circular& buffer) {

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

void Logica_Comando::saida_atuador_aceleracao(Buffer_Circular& buffer) {

    // Neste momento só consumimos; a lógica de “passar isso para o atuador real”
    // ficaria acoplada à Simulação/MQTT na Etapa 2.

    lock_guard<mutex> lock(mtx_acel);
    o_aceleracao = buffer.consumidor_i(ID_C_ACELERA, 0);
}


void Logica_Comando::thread_direita_consome(Buffer_Circular& buffer) {

    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code&) {
        this->direita_consome(buffer);

        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();
}

void Logica_Comando::direita_consome(Buffer_Circular& buffer) {

    lock_guard<mutex> lock(mtx_dir);
    this->dir  = buffer.consumidor_i(ID_C_DIREITA,  0);

}

void Logica_Comando::thread_esquerda_consome(Buffer_Circular& buffer) {

    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, boost::asio::chrono::milliseconds(TEMPO_CICLO));

    std::function<void(const boost::system::error_code&)> handler;
    handler = [&](const boost::system::error_code&) {
        this->esquerda_consome(buffer);

        timer.expires_at(timer.expiry() + boost::asio::chrono::milliseconds(TEMPO_CICLO));
        timer.async_wait(handler);
    };

    timer.async_wait(handler);
    io.run();
}

void Logica_Comando::esquerda_consome(Buffer_Circular& buffer) {

    lock_guard<mutex> lock(mtx_dir);
    this->esq  = buffer.consumidor_i(ID_C_ESQUERDA,0);

}

// -----------------------------------------------------------------------------
// Saída de atuador de direção: c_esquerda soma, c_direita subtrai
// -----------------------------------------------------------------------------
void Logica_Comando::thread_saida_atuador_direcao(Buffer_Circular& buffer) {

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

void Logica_Comando::saida_atuador_direcao(Buffer_Circular& buffer) {
    {

    lock_guard<mutex> lock(mtx_dir);
    lock_guard<mutex> lock2(mtx_esq);
    lock_guard<mutex> lock(mtx_direcao);
    o_direcao = (this->esq)-(this->dir);
    
    }
}

// -----------------------------------------------------------------------------
// Rearme: lê c_rearme do buffer e seta rearme_var_global
// -----------------------------------------------------------------------------
void Logica_Comando::thread_rearme(Buffer_Circular& buffer) {

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

void Logica_Comando::rearme(Buffer_Circular& buffer) {

    c_rearme = buffer.consumidor_b(ID_C_REARME, 0);   // *** USANDO MACRO

    {
        lock_guard<mutex> lock(mtx_rearme);
        rearme_var_global = c_rearme;
    }
}


// main de teste antigo – mantido comentado
/*int main(){
    Buffer_Circular buffer;
    Logica_Comando logica;
    std::atomic<bool> running(true);

    std::thread t_monitor([&]() {
        tarefa_monitoramento_falhas(&buffer, running);
    });

    std::thread t_logica([&]() {
        logica.logica_comando(buffer);
    });

    // Simulação...

    running = false;

    if (t_monitor.joinable()) t_monitor.join();
    if (t_logica.joinable())  t_logica.join();

    return 0;
}*/
