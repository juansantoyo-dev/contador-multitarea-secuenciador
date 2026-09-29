#include "secuenciador.h"
#include "ST7920_parallel.h"
#include "teclado.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =========================
   Configuración del sistema
   ========================= */
#define UPDATE_PERIOD_MS   500U   // Periodo de actualización del contador (ms)
#define LED_SEQ_TIME_MS    250U   // Duración de la secuencia LED (ms)

/* =========================
   Estados del sistema
   ========================= */
static Secuenciador_State_t state = SEQ_STATE_WAIT_PARAMS;

/* =========================
   Variables globales
   ========================= */
static uint16_t NUM_A = 0;
static uint16_t NUM_B = 0;
static uint16_t contador = 0;

static char key_buffer[8];
static uint8_t key_index = 0;

static uint8_t running = 0;
static uint8_t paused = 0;
static uint8_t led_sequence_active = 0;

static uint32_t last_update_ms = 0;
static uint32_t led_seq_end_ms = 0;

static uint8_t current_dir = 0;  // 1 = ascendente, 0 = descendente
static int step_value = 0;        // 1 o 2

/* =========================
   Funciones auxiliares LCD
   ========================= */
static void LCD_ClearAndPrint(const char *msg1, const char *msg2) {
    ST7920_Clear();
    ST7920_SendString(0, 0, (char *)msg1);
    ST7920_SendString(1, 0, (char *)msg2);
}

static void LCD_ShowCounter(void) {
    char txt[32];
    ST7920_Clear();

    // Primera línea: Contador y NUM_A
    snprintf(txt, sizeof(txt), "CNT:%04u  A:%04u", contador, NUM_A);
    ST7920_SendString(0, 0, txt);

    // Segunda línea: NUM_B y estado
    snprintf(txt, sizeof(txt), "B:%04u %s", NUM_B, (paused ? "PAUSA" : "RUN "));
    ST7920_SendString(1, 0, txt);
}

/* =========================
   Entrada de parámetros
   ========================= */
static void AskForParam(uint8_t pos) {
    if (pos == 1) {
        LCD_ClearAndPrint("Ingrese NUM_A:", "0000");
    } else {
        LCD_ClearAndPrint("Ingrese NUM_B:", "0000");
    }
}

static void ResetInputBuffer(void) {
    memset(key_buffer, 0, sizeof(key_buffer));
    key_index = 0;
}

static uint16_t ParseBufferToUInt(void) {
    return (uint16_t)atoi(key_buffer);
}

static uint8_t ValidateRange(void) {
    // Validación: NUM_A > NUM_B
    if (NUM_A == 0 && NUM_B == 0) {
        return 0;
    }
    return (NUM_A > NUM_B) ? 1 : 0;
}

/* =========================
   Secuencia de LEDs
   ========================= */
static void RunLedSequence(void) {
    /*
     * Ejecuta una secuencia luminosa en los LEDs conectados a GPIOB.
     * Patrón: encender en secuencia (LED1, LED2, LED3, LED4) y luego apagar.
     * Se asume:
     *  - LED1: PB0
     *  - LED2: PB1
     *  - LED3: PB2
     *  - LED4: PB3
     */
    
    // LED 1 ON -> OFF
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    
    // LED 2 ON -> OFF
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    
    // LED 3 ON -> OFF
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
    
    // LED 4 ON -> OFF
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);
    
    // Todos apagados
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
}

static void TriggerLedOnMultipleOf10(void) {
    /*
     * Verifica si el contador es múltiplo de 10 (excluyendo 0).
     * Si lo es y no hay secuencia activa, activa la rutina LED.
     * Esta rutina no bloquea el tiempo del contador.
     */
    if ((contador % 10U) == 0U && contador != 0U && !led_sequence_active) {
        led_sequence_active = 1;
        led_seq_end_ms = HAL_GetTick() + LED_SEQ_TIME_MS;
        RunLedSequence();
    }
}

/* =========================
   Control del contador
   ========================= */
static void StopRunningSequence(void) {
    /*
     * Cancela la secuencia activa, limpia la LCD y reinicia el contador a 0000.
     * Tecla '#' activa esta función.
     */
    running = 0;
    paused = 0;
    current_dir = 0;
    step_value = 0;
    state = SEQ_STATE_IDLE;
    contador = 0;
    
    LCD_ClearAndPrint("SECUENCIA", "CANCELADA");
    HAL_Delay(1000);
    LCD_ShowCounter();
}

static void StartSequence(uint8_t dir, int step) {
    /*
     * Inicia una nueva secuencia de conteo.
     * dir: 1 = ascendente, 0 = descendente
     * step: 1 o 2 (incremento por ciclo)
     */
    current_dir = dir;
    step_value = step;
    running = 1;
    paused = 0;
    
    if (dir == 1) {
        // Ascendente: comienza en NUM_B
        contador = NUM_B;
        if (step == 1) {
            state = SEQ_STATE_RUN_ASC_STEP;
        } else {
            state = SEQ_STATE_RUN_ASC_2;
        }
    } else {
        // Descendente: comienza en NUM_A
        contador = NUM_A;
        if (step == 1) {
            state = SEQ_STATE_RUN_DESC_STEP;
        } else {
            state = SEQ_STATE_RUN_DESC_2;
        }
    }
    
    last_update_ms = HAL_GetTick();
    LCD_ShowCounter();
}

static void UpdateCounter(void) {
    /*
     * Actualiza el valor del contador según el estado actual.
     * Se llama periódicamente desde Secuenciador_Update() basado en UPDATE_PERIOD_MS.
     */
    if (!running || paused) {
        return;
    }
    
    switch (state) {
        case SEQ_STATE_RUN_ASC_STEP:
            // Conteo ascendente paso a paso (+1)
            contador += 1;
            if (contador >= NUM_A) {
                contador = NUM_A;  // Fijar al límite superior
                running = 0;
                state = SEQ_STATE_IDLE;
            }
            break;
        
        case SEQ_STATE_RUN_DESC_STEP:
            // Conteo descendente paso a paso (-1)
            contador -= 1;
            if (contador <= NUM_B) {
                contador = NUM_B;  // Fijar al límite inferior
                running = 0;
                state = SEQ_STATE_IDLE;
            }
            break;
        
        case SEQ_STATE_RUN_ASC_2:
            // Conteo ascendente en pasos de 2 (+2)
            contador += 2;
            if (contador >= NUM_A) {
                contador = NUM_A;  // Fijar al límite superior
                running = 0;
                state = SEQ_STATE_IDLE;
            }
            break;
        
        case SEQ_STATE_RUN_DESC_2:
            // Conteo descendente en pasos de 2 (-2)
            contador -= 2;
            if (contador <= NUM_B) {
                contador = NUM_B;  // Fijar al límite inferior
                running = 0;
                state = SEQ_STATE_IDLE;
            }
            break;
        
        default:
            break;
    }
    
    // Verificar si hay que activar la secuencia LED
    TriggerLedOnMultipleOf10();
    
    // Actualizar la pantalla
    LCD_ShowCounter();
}

/* =========================
   Manejo de entrada de teclado
   ========================= */
static void HandleKeypadInput(char ch) {
    /*
     * Máquina de estados que maneja la entrada del teclado.
     * Fase 1: Ingreso de parámetros (NUM_A, NUM_B)
     * Fase 2: Ejecución de comandos (A, B, C, D, *, #)
     */
    
    // Transición inicial desde WAIT_PARAMS a ENTER_NUM_A
    if (state == SEQ_STATE_WAIT_PARAMS) {
        state = SEQ_STATE_ENTER_NUM_A;
        AskForParam(1);
        ResetInputBuffer();
    }
    
    // ===== Fase de entrada de parámetros =====
    if (state == SEQ_STATE_ENTER_NUM_A || state == SEQ_STATE_ENTER_NUM_B) {
        
        // Acepta dígitos '0' a '9'
        if (ch >= '0' && ch <= '9') {
            if (key_index < 4) {
                key_buffer[key_index++] = ch;
                key_buffer[key_index] = '\0';
                
                // Mostrar el número ingresado
                char temp[16];
                snprintf(temp, sizeof(temp), "%4s", key_buffer);
                ST7920_SendString(1, 0, temp);
            }
        }
        // Tecla '#' para borrar el último dígito
        else if (ch == '#') {
            if (key_index > 0) {
                key_buffer[--key_index] = '\0';
                char temp[16];
                snprintf(temp, sizeof(temp), "%4s", key_buffer);
                ST7920_SendString(1, 0, temp);
            }
        }
        // Tecla '*' para confirmar el valor ingresado
        else if (ch == '*') {
            uint16_t value = ParseBufferToUInt();
            
            if (state == SEQ_STATE_ENTER_NUM_A) {
                NUM_A = value;
                state = SEQ_STATE_ENTER_NUM_B;
                ResetInputBuffer();
                AskForParam(2);
            } else {
                // Estamos en ENTER_NUM_B
                NUM_B = value;
                
                // Validar que NUM_A > NUM_B
                if (ValidateRange()) {
                    state = SEQ_STATE_IDLE;
                    contador = NUM_B;  // Inicializa contador con NUM_B
                    LCD_ClearAndPrint("Parametros", "validos OK");
                    HAL_Delay(800);
                    LCD_ShowCounter();
                } else {
                    // Validación fallida: reinicia el proceso
                    state = SEQ_STATE_WAIT_PARAMS;
                    LCD_ClearAndPrint("ERROR:", "A>B requerido");
                    HAL_Delay(1500);
                    AskForParam(1);
                    NUM_A = 0;
                    NUM_B = 0;
                    ResetInputBuffer();
                }
            }
        }
        
        return;  // No procesar más comandos en esta fase
    }
    
    // ===== Fase de ejecución de comandos =====
    if (state == SEQ_STATE_IDLE || state == SEQ_STATE_PAUSED || running) {
        
        switch (ch) {
            case 'A':
                // Tecla 'A': Conteo ascendente paso a paso (+1) desde NUM_B hasta NUM_A
                if (NUM_A > 0 && NUM_B >= 0) {
                    StartSequence(1, 1);
                }
                break;
            
            case 'B':
                // Tecla 'B': Conteo descendente paso a paso (-1) desde NUM_A hasta NUM_B
                if (NUM_A > 0 && NUM_B >= 0) {
                    StartSequence(0, 1);
                }
                break;
            
            case 'C':
                // Tecla 'C': Conteo ascendente en pasos de 2 (+2) desde NUM_B hasta NUM_A
                if (NUM_A > 0 && NUM_B >= 0) {
                    StartSequence(1, 2);
                }
                break;
            
            case 'D':
                // Tecla 'D': Conteo descendente en pasos de 2 (-2) desde NUM_A hasta NUM_B
                if (NUM_A > 0 && NUM_B >= 0) {
                    StartSequence(0, 2);
                }
                break;
            
            case '*':
                // Tecla '*': Pausa / Reanudar
                if (running) {
                    paused = !paused;
                    
                    if (paused) {
                        state = SEQ_STATE_PAUSED;
                    } else {
                        // Restaurar el estado según la dirección y paso
                        if (current_dir == 1) {
                            state = (step_value == 1) ? SEQ_STATE_RUN_ASC_STEP : SEQ_STATE_RUN_ASC_2;
                        } else {
                            state = (step_value == 1) ? SEQ_STATE_RUN_DESC_STEP : SEQ_STATE_RUN_DESC_2;
                        }
                    }
                    
                    LCD_ShowCounter();
                }
                break;
            
            case '#':
                // Tecla '#': Cancela la secuencia y reinicia contador a 0000
                StopRunningSequence();
                break;
            
            default:
                break;
        }
    }
}

/* =========================
   Funciones públicas
   ========================= */
void Secuenciador_Init(void) {
    /*
     * Inicializa el secuenciador:
     * - Configura la LCD ST7920 (ya debe estar inicializada en main)
     * - Inicializa los pines de LEDs en GPIOB (0, 1, 2, 3)
     * - Resetea variables globales
     * - Muestra pantalla inicial
     */
    
    // Configurar pines de LEDs (GPIOB 0..3)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // Apagar todos los LEDs inicialmente
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
    
    // Resetear variables globales
    state = SEQ_STATE_WAIT_PARAMS;
    contador = 0;
    running = 0;
    paused = 0;
    led_sequence_active = 0;
    NUM_A = 0;
    NUM_B = 0;
    
    // Mostrar pantalla inicial
    LCD_ClearAndPrint("Secuenciador", "Multitarea");
    HAL_Delay(1500);
    
    // Solicitar parámetros
    AskForParam(1);
}

void Secuenciador_Update(void) {
    /*
     * Función a llamar periódicamente en el loop principal.
     * Recomendado: llamar cada 10-20 ms o más frecuentemente.
     * 
     * Maneja:
     * - Lectura del teclado
     * - Actualización del contador (basada en timer de UPDATE_PERIOD_MS)
     * - Control de la secuencia LED
     */
    
    // Leer tecla del teclado
    char ch = teclado();
    if (ch != '\0') {
        HandleKeypadInput(ch);
    }
    
    // Actualizar contador si es necesario
    uint32_t now = HAL_GetTick();
    
    if (running && !paused) {
        if ((now - last_update_ms) >= UPDATE_PERIOD_MS) {
            last_update_ms = now;
            UpdateCounter();
        }
    }
    
    // Resetear flag de secuencia LED cuando termine
    if (led_sequence_active && (now >= led_seq_end_ms)) {
        led_sequence_active = 0;
    }
}

uint16_t Secuenciador_GetCounter(void) {
    return contador;
}

uint16_t Secuenciador_GetNumA(void) {
    return NUM_A;
}

uint16_t Secuenciador_GetNumB(void) {
    return NUM_B;
}

uint8_t Secuenciador_IsRunning(void) {
    return running;
}

uint8_t Secuenciador_IsPaused(void) {
    return paused;
}

Secuenciador_State_t Secuenciador_GetState(void) {
    return state;
}
