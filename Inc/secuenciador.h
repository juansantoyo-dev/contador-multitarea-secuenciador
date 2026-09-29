#ifndef SECUENCIADOR_H_
#define SECUENCIADOR_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ===========================
   Estados del Secuenciador
   =========================== */

typedef enum {
    SEQ_STATE_WAIT_PARAMS,       // Esperando entrada de NUM_A
    SEQ_STATE_ENTER_NUM_A,       // Ingresando NUM_A
    SEQ_STATE_ENTER_NUM_B,       // Ingresando NUM_B
    SEQ_STATE_IDLE,              // Ocioso, esperando comando
    SEQ_STATE_RUN_ASC_STEP,      // Conteo ascendente +1
    SEQ_STATE_RUN_DESC_STEP,     // Conteo descendente -1
    SEQ_STATE_RUN_ASC_2,         // Conteo ascendente +2
    SEQ_STATE_RUN_DESC_2,        // Conteo descendente -2
    SEQ_STATE_PAUSED             // Pausado
} Secuenciador_State_t;

/* ===========================
   Funciones Públicas
   =========================== */

void Secuenciador_Init(void);
void Secuenciador_Update(void);
uint16_t Secuenciador_GetCounter(void);
uint16_t Secuenciador_GetNumA(void);
uint16_t Secuenciador_GetNumB(void);
uint8_t Secuenciador_IsRunning(void);
uint8_t Secuenciador_IsPaused(void);
Secuenciador_State_t Secuenciador_GetState(void);

#endif /* SECUENCIADOR_H_ */
