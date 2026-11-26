/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Bleidy Carolina Garcia
 * @brief          : Main program body. Tarea
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
#include "system.h"
#include "heartbeat.h"
#include "app.h"

/* TIM3 handle — must be global so stm32f4xx_it.c can access it */
UART_HandleTypeDef huart2;

/* Private function prototypes */
// static void SystemClock_Config(void); Ya está definida en system.h
static void gpio_Init(void);
static void usart2_Init(void);

//
volatile uint8_t send_msg_flag = 0;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    heartbeat_init(250U);   /* toggle every 250 ms = 2 Hz blink */
    app_init();             /* provided by the selected stage */

    while (1) {
        app_loop();         /* provided by the selected stage */
    }
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
