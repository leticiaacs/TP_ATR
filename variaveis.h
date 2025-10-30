#ifndef VARIAVEIS_H
#define VARIAVEIS_H

// Inclui o buffer.h para termos acesso às definições de N_VAR_I e N_VAR_B
#include "buffer.h"

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
#define ID_I_TEMP 3

// Atuadores (Tabela 1)
#define ID_O_ACELERACAO 4
#define ID_O_DIR 5

// Setpoints (criados para o Planejamento de Rota)
#define ID_SP_POS_X 6 // Setpoint de Posição X
#define ID_SP_POS_Y 7 // Setpoint de Posição Y

// Reservado (temos 9, usamos 8)
#define ID_RESERVADO_I 8

// ATENÇÃO: O código do buffer.cpp (produz_b, consome_b) usa
// o índice [id_var - N_VAR_I].
// Portanto, os IDs aqui devem ser (indice_real + N_VAR_I).

// Sensores (Tabela 1)
#define ID_I_FAIL_ELETRICA (0 + N_VAR_I)
#define ID_I_FAIL_HIDRAULICA (1 + N_VAR_I)

// Estados (Tabela 2)
#define ID_E_DEFEITO (2 + N_VAR_I)
#define ID_E_AUTOMATICO (3 + N_VAR_I)

// Comandos (Tabela 2)
#define ID_C_AUTOMATICO (4 + N_VAR_I)
#define ID_C_MANUAL (5 + N_VAR_I)
#define ID_C_REARME (6 + N_VAR_I)

#endif