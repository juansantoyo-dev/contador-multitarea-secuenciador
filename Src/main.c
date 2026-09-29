#include "stm32f4xx_hal.h"
#include "ST7920_parallel.h"
#include "secuenciador.h"
#include <stdio.h>

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void SystemClock_Config(void);
void MX_GPIO_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    // =============================================================
    // INICIALIZAR PANTALLA ST7920 Y TECLADO
    // =============================================================
    ST7920_Init();
    
    // =============================================================
    // INICIALIZAR SECUENCIADOR
    // =============================================================
    Secuenciador_Init();

    // =============================================================
    // LOOP PRINCIPAL
    // =============================================================
    while (1) {
        // Actualizar secuenciador: lee teclado, actualiza contador, controla LEDs
        Secuenciador_Update();
    }
}


// =============================================================
// CONFIGURACION DE GPIO
// =============================================================

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Habilitar relojes
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    // =============================================================
    // PUERTO C - CONTROL LCD
    // RS = PC9
    // RW = PC10
    // E  = PC11
    // =============================================================
    GPIO_InitStruct.Pin = RS_PIN | RW_PIN | E_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // =============================================================
    // PUERTO D - DATOS Y RESET LCD
    // RST = PD3
    // DB7 = PD4
    // DB6 = PD5
    // DB5 = PD6
    // DB4 = PD7
    // =============================================================
    GPIO_InitStruct.Pin = RST_PIN | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // =============================================================
    // PUERTO E - COLUMNAS DEL TECLADO MATRICIAL
    // COL1 = PE2
    // COL2 = PE4
    // COL3 = PE5
    // COL4 = PE6
    // ROW1 = PE3
    // =============================================================
    GPIO_InitStruct.Pin = TECLADO_COL1_PIN | TECLADO_COL2_PIN | TECLADO_COL3_PIN | TECLADO_COL4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // Fila 1 en GPIOE
    GPIO_InitStruct.Pin = TECLADO_ROW1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    // =============================================================
    // PUERTO F - FILAS DEL TECLADO MATRICIAL
    // ROW2 = PF8
    // ROW3 = PF7
    // ROW4 = PF9
    // =============================================================
    GPIO_InitStruct.Pin = TECLADO_ROW2_PIN | TECLADO_ROW3_PIN | TECLADO_ROW4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // =============================================================
    // PUERTO B - LEDs INDICADORES
    // LED1 = PB0
    // LED2 = PB1
    // LED3 = PB2
    // LED4 = PB3
    // =============================================================
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Desactivar todas las columnas del teclado al iniciar
    HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);
}


// =============================================================
// CONFIGURACION DEL CLOCK
// =============================================================

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // =============================================================
    // HSI = 16 MHz
    // =============================================================
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // =============================================================
    // CONFIGURACION DE CLOCKS
    // =============================================================
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}
