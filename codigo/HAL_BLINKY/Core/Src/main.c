/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Bleidy Carolina Garcia
 * @brief          : Main program body. HAL Blinky — LED on PA5 toggled every 250 ms by TIM3
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */



#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"
#include <stdint.h>

/* TIM3 handle — must be global so stm32f4xx_it.c can access it */
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart2;

/* Private function prototypes */
static void SystemClock_Config(void);
static void gpio_Init(void);
static void tim3_Init(void);
static void usart2_Init(void);

//
volatile uint8_t send_msg_flag = 0;

int main(void)
{
    HAL_Init();           /* initialize HAL: SysTick, cache, priority grouping */
    SystemClock_Config(); /* configure clock tree: HSI at 16 MHz               */
    gpio_Init();          /* configure PA5 as push-pull output                  */
    tim3_Init();          /* configure TIM3: update event every 250 ms          */
    usart2_Init();
    while (1)
    {
        /* application loop — LED toggling happens in the callback */
        if (send_msg_flag){
            send_msg_flag = 0;
            char msg[] = "Hello from STM32\r\n";
            HAL_UART_Transmit(&huart2,(uint8_t *)msg, sizeof(msg)-1,100);
        }
    }
}

/*
 * SystemClock_Config
 * Uses HSI internal oscillator at 16 MHz
 * No PLL — simplest possible clock configuration
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSI is already on at reset — confirm and use it */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* Select HSI as SYSCLK — all bus dividers set to 1 */
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_HCLK   |
                                       RCC_CLOCKTYPE_PCLK1  |
                                       RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;   /* HCLK  = 16 MHz */
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;     /* APB1  = 16 MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;     /* APB2  = 16 MHz */

    /* FLASH_LATENCY_0 = zero wait states, correct for 16 MHz */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

/*
 * gpio_Init
 * Configures PA5 as push-pull output — onboard LED on Nucleo board
 */
static void gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIOA clock on AHB1 bus
       Same as bare-metal: RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Configure PA5 */
    GPIO_InitStruct.Pin   = GPIO_PIN_5;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/*
 * tim3_Init
 * Configures TIM3 to generate an update event every 250 ms
 *
 * Clock chain:
 *   HSI (16 MHz) → APB1 (16 MHz) → TIM3 clock (16 MHz)
 *
 * PSC = 15999  →  tick = 16,000,000 / (15999 + 1) = 1,000 Hz  (1 ms per tick)
 * ARR = 249    →  period = (249 + 1) x 1 ms = 250 ms
 */
static void tim3_Init(void)
{
    /* Enable TIM3 clock on APB1 bus */
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* Configure TIM3 base */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 15999;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = 249;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&htim3);

    /* Start TIM3 in interrupt mode — enables the update event interrupt */
    HAL_TIM_Base_Start_IT(&htim3);

    /* Enable TIM3 interrupt line in the NVIC */
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

/*
 * HAL_TIM_PeriodElapsedCallback
 * Called automatically by HAL_TIM_IRQHandler() every time a timer
 * update event fires. Shared by all timers — always check htim->Instance.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        send_msg_flag = 1;
    }
}


// usart2_Init
static void usart2_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Step 1: enable clocks — both USART2 and GPIOA */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();   /* already called in gpio_Init() — safe to call again */

    /* Step 2: configure PA2 (TX) and PA3 (RX) as alternate function
       AF7 is USART2 on the STM32F411 — see the alternate function table in the datasheet */
    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* Step 3: configure USART2 */
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 115200;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}
