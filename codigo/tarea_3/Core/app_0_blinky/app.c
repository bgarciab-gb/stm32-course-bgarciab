/* Core/app_0_blinky/app.c — Stage 0: no UART, todavía*/
#include "app.h"

#include "stm32f4xx_hal.h"
#include "heartbeat.h"

void app_init(void)
{
    /* */
    /*HAL_Delay(5000U);
    heartbeat_set_period(1000U);*/
}

void app_loop(void)
{
    /* nothing: the heartbeat runs from the TIM2 interrupt */
}