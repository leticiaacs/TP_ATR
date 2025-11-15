#include "monitoramento_de_falhas.h"
#include "variaveis.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace std;
mutex mtx_falha_eletrica;
mutex mtx_falha_hidraulica;
mutex mtx_falha_temp_alerta;
mutex mtx_falha_temp_defeito;
mutex mtx_defeito;
mutex mtx_rearme;
bool rearme_var_global;
bool defeito_var_global;
bool falha_temp_alerta_global;
bool falha_temp_defeito_global;
bool falha_eletrica_global;
bool falha_hidraulica_global;

void tarefa_monitoramento_falhas(Buffer_Circular* buffer, atomic<bool>& running) {

    cout << "[Monitoramento Falhas] Tarefa iniciada." << endl;

    bool falha_eletrica = false;
    bool falha_hidraulica = false;
    bool temp_alta = false;

    mtx_defeito.lock();
    defeito_var_global = false;
    mtx_defeito.unlock();

    mtx_falha_temp_alerta.lock();
    falha_temp_alerta_global = false;
    mtx_falha_temp_alerta.unlock();
    mtx_falha_temp_defeito.lock();
    falha_temp_defeito_global = false;
    mtx_falha_temp_defeito.unlock();
    mtx_falha_eletrica.lock();
    falha_eletrica_global = false;
    mtx_falha_eletrica.unlock();
    mtx_falha_hidraulica.lock();
    falha_hidraulica_global = false;
    mtx_falha_hidraulica.unlock();

   // Threads para cada sensor
    thread t_falha_eletrica(thread_falha_eletrica, ref(buffer), ref(running), ref(falha_eletrica));
    thread t_falha_hidraulica(thread_falha_hidraulica, ref(buffer), ref(running), ref(falha_hidraulica));
    thread t_temp(thread_temp, ref(buffer), ref(running), ref(temp_alta));
    thread t_rearme(thread_rearme, ref(buffer), ref(running),ref(falha_eletrica), ref(falha_hidraulica), ref(temp_alta));

    
    // Espera todas terminarem
    t_falha_eletrica.join();
    t_falha_hidraulica.join();
    t_temp.join();
    t_rearme.join();

    cout << "[Monitoramento Falhas] Tarefa encerrada." << endl;

}

void thread_falha_eletrica(Buffer_Circular* buffer, atomic<bool>& running, bool &falha_eletrica) {

    while(running) {
        // --- Leitura dos sensores ---
        bool falha_eletrica = buffer->consumidor_b(10,0);

        // --- Lógica de verificação ---
        if (falha_eletrica) {
            cout << "[Monitoramento Falhas] Falha elétrica detectada!" << endl;
            mtx_defeito.lock();
            defeito_var_global = true;
            mtx_defeito.unlock();
            mtx_falha_eletrica.lock();
            falha_eletrica_global = true;
            mtx_falha_eletrica.unlock();
        } else{
            mtx_falha_eletrica.lock();
            falha_eletrica_global = false;
            mtx_falha_eletrica.unlock();
        }
        //this_thread::sleep_for(chrono::milliseconds(500));
    }
};

void thread_falha_hidraulica(Buffer_Circular* buffer, atomic<bool>& running, bool &falha_hidraulica) {
        while(running) {
        // --- Leitura dos sensores ---
        falha_hidraulica = buffer->consumidor_b(11,0);

        if (falha_hidraulica) {
            cout << "[Monitoramento Falhas] Falha hidráulica detectada!" << endl;
            mtx_defeito.lock();
            defeito_var_global = true;
            mtx_defeito.unlock();
            mtx_falha_hidraulica.lock();
            falha_hidraulica_global = true;
            mtx_falha_hidraulica.unlock();
        }   else {
            mtx_falha_hidraulica.lock();
            falha_hidraulica_global = false;
            mtx_falha_hidraulica.unlock();
        }
        //this_thread::sleep_for(chrono::milliseconds(500));

    }
};

void thread_temp(Buffer_Circular* buffer, atomic<bool>& running, bool &temp_defeito) {
        
        while(running) {
        // --- Leitura dos sensores ---
        int temperatura = 0;
        temperatura = buffer->consumidor_i(0,0);
    
        if (temperatura > 120) {
            cout << "[Monitoramento Falhas] ALERTA CRÍTICO: Temperatura > 120°C" << endl;
            mtx_defeito.lock();
            defeito_var_global = true;
            mtx_defeito.unlock();
            mtx_falha_temp_defeito.lock();
            falha_temp_defeito_global = true;
            mtx_falha_temp_defeito.unlock();
            temp_defeito = true;
        } else if (temperatura > 95) {
            cout << "[Monitoramento Falhas] Alerta: Temperatura alta (>95°C)" << endl;
            mtx_falha_temp_alerta.lock();
            falha_temp_alerta_global = true;
            mtx_falha_temp_alerta.unlock();
            temp_defeito = false;
        } else{
            temp_defeito = false;
        }
        //this_thread::sleep_for(chrono::milliseconds(500));
    }
};


void thread_rearme(Buffer_Circular* buffer, atomic<bool>& running, bool &falha_eletrica, bool &falha_hidraulica, bool &temp_defeito) {
        while(running) {

        mtx_rearme.lock();
        if ((rearme_var_global)&&(!falha_eletrica)&&(!falha_hidraulica)&&(!temp_defeito)){
            mtx_defeito.lock();
            defeito_var_global = false;
            mtx_defeito.unlock();
        }
        mtx_rearme.unlock();

        //this_thread::sleep_for(chrono::milliseconds(500));
    }
};