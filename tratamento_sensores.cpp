
#include "tratamento_sensores.h"
#include "buffer.h"

    void Tratamento_Sensores::tratamento_sensores(Buffer_Circular& buffer, int i_posicao_x, int i_posicao_y, int i_angulo_x ){

        //essa função vai ser chamada UMA vez no main e receberá os dados dos sensores por meio de um pub/sub MQTT.

        // Threads para cada sensor
        thread t_x(&Tratamento_Sensores::thread_sensor_px, this, ref(buffer), i_posicao_x); //uso do ref para passar buffer como referência e nao cópia
        thread t_y(&Tratamento_Sensores::thread_sensor_py, this, ref(buffer), i_posicao_y); // uso do this porque o método é um método da própria clase
        thread t_ang(&Tratamento_Sensores::thread_sensor_ax, this, ref(buffer), i_angulo_x);

        // Espera todas terminarem
        t_x.join();
        t_y.join();
        t_ang.join();
        
    }

    void Tratamento_Sensores::thread_sensor_px(Buffer_Circular& buffer, int i_posicao_x){

        //Precisará ser PERIÓDICA. A cada novo valor recebido, essa função será chamada.
        this->constroi_vetor(v_i_posicao_x, i_posicao_x);
        if(v_i_posicao_x.size()==M){
        soma_i_posicao_x = this->filtro(v_i_posicao_x);
        this->adiciona_buffer(buffer, soma_i_posicao_x, 0);
        }
    }

     void Tratamento_Sensores::thread_sensor_py(Buffer_Circular& buffer, int i_posicao_y){

        //Precisará ser PERIÓDICA. A cada novo valor recebido, essa função será chamada.
        this->constroi_vetor(v_i_posicao_y, i_posicao_y);
        if(v_i_posicao_y.size()==M){
        soma_i_posicao_y = this->filtro(v_i_posicao_y);
        this->adiciona_buffer(buffer, soma_i_posicao_y, 0);
        }
    }

     void Tratamento_Sensores::thread_sensor_ax(Buffer_Circular& buffer, int i_angulo_x){

        //Precisará ser PERIÓDICA. A cada novo valor recebido, essa função será chamada.
        this->constroi_vetor(v_i_angulo_x, i_angulo_x);
        if(v_i_angulo_x.size()==M){
        soma_i_angulo_x = this->filtro(v_i_angulo_x);
        this->adiciona_buffer(buffer, soma_i_angulo_x, 0);
        }
    }

    void Tratamento_Sensores::constroi_vetor(vector<int>& v, int var){
        if(v.size()==M){
            v.erase(v.begin());
        }
        v.push_back(var);
    }

    int Tratamento_Sensores::filtro(vector<int>& v){

        int soma = 0;
        int resultado = 0;

        for(int i = 0; i<M; i++){
            soma += v[i];
        }

        resultado = soma/M;
        return resultado;

    }

    void Tratamento_Sensores::adiciona_buffer(Buffer_Circular& buffer, int resultado, int id_sensor){
        buffer.produtor_i(resultado, id_sensor);
    }
