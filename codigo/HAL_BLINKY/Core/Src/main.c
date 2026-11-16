/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Bleidy Carolina Garcia
 * @brief          : Main program body
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


#include <stdint.h>
#include "stm32f411xe.h"
#include "stm32f4xx.h"

//-----------------Definición de variables----------------------------------


//-----------------Definición de funciones----------------------------------


//--------------------------main-----------------------------------
int main(void)
{
    init_gpio();
    init_timers();
    init_EXTI();

    while(1) {
        

    }
    return 0;
}