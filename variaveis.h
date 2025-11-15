#ifndef VARIAVEIS_H
#define VARIAVEIS_H

// Inclui o buffer.h para termos acesso às definições de N_VAR_I e N_VAR_B
#include "buffer.h"

//varíaveis para comunicação via evento
extern bool defeito_var_global;
extern bool rearme_var_global;
extern bool falha_eletrica_global;
extern bool falha_hidraulica_global;
extern bool falha_temp_alerta_global;
extern bool falha_temp_defeito_global;
extern mutex mtx_defeito;
extern mutex mtx_rearme;
extern mutex mtx_falha_eletrica;
extern mutex mtx_falha_hidraulica;
extern mutex mtx_falha_temp_alerta;
extern mutex mtx_falha_temp_defeito;

/*
 * Este arquivo centraliza os IDs de todas as variáveis
 * que serão usadas no Buffer_Circular, conforme as Tabelas 1 e 2
 * do documento do trabalho.
 */

// --- Índices para variáveis INT (dados_i) ---
// Total: N_VAR_I = 9

// Sensores (Tabela 1)
#define ID_I_POS_X 0
#define ID_I_POS_Y 1
#define ID_I_ANG_X 2
//#define ID_I_TEMP 3


// Atuadores (Tabela 1)
//#define ID_O_ACELERACAO 4
//#define ID_O_DIR 5

// Setpoints (criados para o Planejamento de Rota)
//#define ID_SP_POS_X 6 // Setpoint de Posição X
//#define ID_SP_POS_Y 7 // Setpoint de Posição Y
#define ID_SP_VEL 6
#define ID_SP_ANG_X 7 // Setpoint do angulo de X

// Reservado (temos 9, usamos 8)
//#define ID_RESERVADO_I 8

// ATENÇÃO: O código do buffer.cpp (produz_b, consome_b) usa
// o índice [id_var - N_VAR_I].
// Portanto, os IDs aqui devem ser (indice_real + N_VAR_I).

// Sensores (Tabela 1)
//#define ID_I_FAIL_ELETRICA (0 + N_VAR_I)
//#define ID_I_FAIL_HIDRAULICA (1 + N_VAR_I)

// Estados (Tabela 2)
#define ID_E_DEFEITO (0 + N_VAR_I)
#define ID_E_AUTOMATICO (1 + N_VAR_I)

// Comandos (Tabela 2)
#define ID_C_AUTOMATICO (2 + N_VAR_I)
#define ID_C_MANUAL (3 + N_VAR_I)
#define ID_C_REARME (4 + N_VAR_I)
#define ID_C_ACELERA 3
#define ID_C_DIREITA 4
#define ID_C_ESQUERDA 5


#endif