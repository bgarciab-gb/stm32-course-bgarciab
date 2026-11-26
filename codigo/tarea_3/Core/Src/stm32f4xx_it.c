/*
*stm32f4xx_it.c
*Interrupt service routines
*Author: Bleidy Carolina Garcia B. bgarciab@unal.edu.co
*/

#include "stm32f4xx_hal.h"

/* Declare the TIM3 handle — defined in main.c */
extern TIM_HandleTypeDef htim3;
/* SysTick handler — required by HAL for HAL_Delay() and timeouts */
void SysTick_Handler(void){
HAL_IncTick();
}
