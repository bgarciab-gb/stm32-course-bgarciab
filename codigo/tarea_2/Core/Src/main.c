/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Bleidy Carolina Garcia Blanco. bgarciab@unal.edu.co
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
 *
 *
 * ========================== PINES ========================== *
 *
 ** Capturar DATA y CLK del encoder:
 * PAO --> TIM2_CH1 (AF01)  [CLK]
 * PA1 --> TIM2_CH2 (AF01)  [DATA]
 * 
 ** Señales 'PWM': 
 * PB0 --> TIM3_CH3 (AF02) [variar voltaje]
 * PC7 --> TIM3_CH2 (AF02) [led rojo]
 * PC9 --> TIM3_CH4 (AF02) [led verde]
 * PC6 --> TIM3_CH1 (AF02) [led azul]
 *
 ** Capture/compare de un pwm:
 * PB9 --> TIM4_CH4 (AF02)
 *
 ** Blinky:
 * PH1 --> TIM11
 *
 ** Pulsador (SW): 
 * PC0 --> EXTI0
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 *
 *
 *
 * =========================== FSM =========================== *
 *
 */

#include <stdint.h>
#include "stm32f411xe.h"
#include "stm32f4xx.h"

//-----------------Definición de variables----------------------------------


volatile uint32_t time_ms = 0; // almacena los ms para el debounce
volatile uint8_t button_flag = 0; //Bandera que deja la ISR del pulsador
volatile uint8_t blink_flag = 0; //Bandera que deja la ISR del TIM11

int32_t arr_value = 1999; //ARR que tiene el TIM3 (periodo = ARR + 1 us): (1999 + 1) us -> 500 Hz.
int32_t ccr_value_red = 0; //CCR --> ancho del pulso led rojo
int32_t ccr_value_green = 0; //CCR --> ancho del pulso led verde
int32_t ccr_value_blue = 0; //CCR --> ancho del pulso led azul
int32_t ccr_value_voltage = 0; //CCR --> ancho del pulso para generar señal de voltaje

volatile uint16_t capture_1 = 0;
volatile uint16_t capture_2 = 0;
volatile int16_t signal_period = 0;

//-----------------Cabecera de funciones----------------------------------

// Inicialización
void init_gpio(void);
void init_timer2_encoder(void);
void init_timer3_PWM(void);
void init_timer4_capture(void);
void init_timer10(void); //Debounce
void init_timer11(void); //Blinky
void init_EXTI(void);



//--------------------------main-----------------------------------
int main(void)
{
    void init_gpio(void);
    void init_EXTI(void);
    void init_timer3_PWM(void);
    void init_timer2_encoder(void);
    void init_timer4_capture(void);
    void init_timer10(void); //Debounce
    void init_timer11(void); //Blinky
    

    while(1) {
        

    }
    return 0;
}


//===================== FUNCIONES DE INICIALIZACIÓN =====================//

void init_gpio(void){

    // Habilitar el reloj para los puertos GPIO (A, B, C y H)

    RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOHEN); // Limpiar los bits de habilitación del reloj para GPIOA, GPIOB, GPIOC y GPIOH
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // Habilitar el reloj para GPIOA
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; // Habilitar el reloj para GPIOC
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // Habilitar el reloj para GPIOB
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; // Habilitar el reloj para GPIOH
    
    // Configurando GPIOH - pin 1 LED blinky, breadboard

    GPIOH->MODER    &= ~(GPIO_MODER_MODE1);     //Limpiar             
    GPIOH->MODER    |= (GPIO_MODER_MODE1_0);    //Configurar PH0 como salida  
    GPIOH->OTYPER   &= ~(GPIO_OTYPER_OT1);      // Salida push-pull                              
    GPIOH->OSPEEDR  &= ~(GPIO_OSPEEDR_OSPEED1);   // velocidad baja                             
    GPIOH->PUPDR    &= ~(GPIO_PUPDR_PUPD1);       // sin resisencia pull-up y sin resistencia pull-down                               
    GPIOH->ODR      &= ~(GPIO_ODR_OD1);  // Inicializar el pin PH1 en 0 logico (led apagado)

    // Configurando GPIOC pin 0 (EXTI pulsador):
    
    GPIOC->MODER    &= ~(GPIO_MODER_MODE0);   // Entrada
    GPIOC->PUPDR    &= ~(GPIO_PUPDR_PUPD0);   // Limpiar la resistencia de pull up--pull down

    /* Pines salida PWM */

    // Configurar PB0 como función alternativa AF02
    GPIOB->MODER &= ~GPIO_MODER_MODE0; //Limpiar (00)
    GPIOB->MODER |= GPIO_MODER_MODE0_1; //(10) Función alternativa
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFRL0; //Limpiar
    GPIOB->AFR[0] |= GPIO_AFRL_AFRL0_1; //(0010 AF02)

    // Configurar PC6, PC7, PC9 como función alternativa AF02
    GPIOC->MODER &= ~(GPIO_MODER_MODE6|GPIO_MODER_MODE7|GPIO_MODER_MODE9); //Limpiar (00)
    GPIOC->MODER |= (GPIO_MODER_MODE6_1|GPIO_MODER_MODE7_1|GPIO_MODER_MODE9_1); //(10) Función alternativa
    GPIOC->AFR[0] &= ~(GPIO_AFRL_AFRL6|GPIO_AFRL_AFRL7); //Limpiar
    GPIOC->AFR[1] &= ~(GPIO_AFRH_AFRH1 ); //Limpiamos AFHR9
    GPIOC->AFR[0] |= (GPIO_AFRL_AFRL6_1|GPIO_AFRL_AFRL7_1); //(0010 AF02)
    GPIOC->AFR[1] |= GPIO_AFRH_AFRH1_1; //(0010 AF02)

    /* Pines para capture/compare */

    // Encoder (CLK: PA0 AF01) (DT: PA1 AF01)
    GPIOA->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1); //Limpiar (00)
    GPIOA->MODER |= (GPIO_MODER_MODE0_1 | GPIO_MODER_MODE1_1); //(10) Función alternativa
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFRL0 | GPIO_AFRL_AFRL1); //Limpiamos
    GPIOA->AFR[0] |= (GPIO_AFRL_AFRL0_0 | GPIO_AFRL_AFRL1_0); //(0001 AF01)
    
    // Capturar frecuencia (PB9 AF02)
    GPIOB->MODER &= ~GPIO_MODER_MODE9; //Limpiar (00)
    GPIOB->MODER |= GPIO_MODER_MODE9_1; //(10) Función alternativa
    GPIOB->AFR[1] &= ~GPIO_AFRH_AFRH1; //Limpiar
    GPIOB->AFR[0] |= GPIO_AFRH_AFRH1_1; //(0010 AF02)
                                        
}

void init_EXTI(void){
    RCC->APB2ENR &= ~(RCC_APB2ENR_SYSCFGEN); // Deshabilitar el reloj para SYSCFG
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; // Habilitar el reloj para SYSCFG

    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0; // Limpiar
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PC; // La línea EXTI3 viene del puerto C (PC0)

    EXTI->RTSR &= ~EXTI_RTSR_TR0; // desactivar interrupción por flanco de subida
    EXTI->FTSR |= EXTI_FTSR_TR0; // interrupción por flanco de bajada

    // EXTI->PR = EXTI_PR_PR0; // Limpiar la bandera pendiente (se limpia escribiendo 1)
    EXTI->IMR |= EXTI_IMR_MR0; // 'Desenmascarar' EXTI0 (habilitar la interrupción)

    __NVIC_EnableIRQ(EXTI0_IRQn);   // Habilitar en el NVIC la interrupción proveniente de EXTI0 (PC0)
}

void init_timer3_PWM(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; //señal de reloj del TIM3

    TIM3->PSC = 16-1; //Tick de 1 us
	TIM3->ARR = arr_value; //Periodo: (1999 + 1) us Frecuencia 500 Hz
	TIM3->EGR |= TIM_EGR_UG; //Fuerza una actualización para cargar PSC, ARR y CCR1 antes de arrancar
	TIM3->CR1 |= TIM_CR1_ARPE; //Preload del ARR

    //Canal 1 LED AZUL:
    TIM3->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); //110: PWM modo 1
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE; //CCR1 con preload
    TIM3->CCER |= TIM_CCER_CC1E; //Habilita la salida del canal 1 al pin
    TIM3->CCR1 = ccr_value_blue; //Ancho del pulso

    //Canal 2 LED ROJO:
    TIM3->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2); //110: PWM modo 1
    TIM3->CCMR1 |= TIM_CCMR1_OC2PE; //CCR1 con preload
    TIM3->CCER |= TIM_CCER_CC2E; //Habilita la salida del canal 1 al pin
    TIM3->CCR2 = ccr_value_red; //Ancho del pulso

    //Canal 3 SEÑAL DE VOLTAJE:
    TIM3->CCMR2 |= (TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2); //110: PWM modo 1
    TIM3->CCMR2 |= TIM_CCMR2_OC3PE; //CCR2 con preload
    TIM3->CCER |= TIM_CCER_CC3E; //Habilita la salida del canal 1 al pin
    TIM3->CCR3 = ccr_value_voltage; //Ancho del pulso

    //Canal 4 LED VERDE:
    TIM3->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2); //110: PWM modo 1
    TIM3->CCMR2 |= TIM_CCMR2_OC4PE; //CCR2 con preload
    TIM3->CCER |= TIM_CCER_CC4E; //Habilita la salida del canal 1 al pin
    TIM3->CCR4 = ccr_value_green; //Ancho del pulso

    //Arrancar el contador del timer
    TIM3->CR1 |= TIM_CR1_CEN; //siempre de último
}

void init_timer2_encoder(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //Reloj del TIM2

    TIM2->ARR = 0xFFFFFFFF; //TIM2 es de 32 bit: no se desborda en uso normal
    TIM2->CR1 |= TIM_CR1_ARPE; //Preload del ARR

    TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0); //01: CH1 y CH2 configurados como input. CC1 y CC2 mapeados a TI1 y TI2
    TIM2->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F); //Filtro máximo: rechaza el rebote del encoder
    TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E); //Habilita la captura de los dos canales

    TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); //011: modo encoder 3, cuenta los flancos de TI1 y TI2 -Slave mode
    TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador
}

void init_timer4_capture(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //Reloj del TIM2

    TIM4->ARR = 0xFFFF; //maximo
    TIM4->PSC = 16-1; //cuenta en us
    TIM4->CR1 |= TIM_CR1_ARPE; //Preload del ARR

    // Canal 4:
    TIM4->CCMR2 |= (TIM_CCMR2_CC4S_0); //01: CH4 configurado como input. CC4 mapeado a TI4.
    
    TIM4->CCER &= ~(TIM_CCER_CC4P|TIM_CCER_CC4NP);
    TIM4->CCER |= (TIM_CCER_CC4E); //Habilita la captura del CH4

    TIM4->DIER |= (TIM_DIER_CC4IE);

    TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); //011: modo encoder 3, cuenta los flancos de TI1 y TI2 -Slave mode
    TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador

    __NVIC_EnableIRQ(TIM4_IRQn);

}

void init_timer10(void){

    RCC->APB2ENR &= ~(RCC_APB2ENR_TIM11EN); //limpiar
    RCC->APB2ENR |= (RCC_APB2ENR_TIM10EN_Msk); // Habilitar señal de reloj para TIM10

    TIM10->PSC = 16-1;   // 16 MHz/16 = 1 MHz --> us
	TIM10->ARR = 1000-1;    // 1000 us --> 1 ms
	TIM10->CR1 |= TIM_CR1_ARPE; //Habilitar la precarga del auto-reload
	TIM10->EGR |= TIM_EGR_UG; //
	TIM10->SR &= ~TIM_SR_UIF; //Update generation levanta UIF: se baja para que no cuente como un tick falso
	TIM10->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización
	__NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn ); //Habilita la interrupción en el NVIC

	TIM10->CR1 |= TIM_CR1_CEN; //Arranca el contador
}

void init_timer11(void){

    RCC->APB2ENR &= ~(RCC_APB2ENR_TIM11EN); //limpiar
    RCC->APB2ENR |= (RCC_APB2ENR_TIM11EN_Msk); // Habilitar señal de reloj para TIM11

    TIM11->PSC = 16000-1;   // 16 MHz/16000 = 1 kHz
	TIM11->ARR = 500-1;    // 500 ms
	TIM11->CR1 |= TIM_CR1_ARPE; //Habilitar la precarga del auto-reload
	TIM11->EGR |= TIM_EGR_UG; //
	TIM11->SR &= ~TIM_SR_UIF; //Update generation levanta UIF: se baja para que no cuente como un tick falso
	TIM11->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización
	__NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn); //Habilita la interrupción en el NVIC

	TIM11->CR1 |= TIM_CR1_CEN; //Arranca el contador
}



//===================== HANDLERS INTERRUPCIONES =====================//

/* Handler para la interrupción de TIM10 (blinky) */
void TIM1_UP_TIM10_IRQHandler(void){
    if (TIM10->SR & TIM_SR_UIF) { // Pregunta si se alzó la bandera
        TIM10->SR &= ~TIM_SR_UIF; // Limpiar la bandera de interrupción
        time_ms++;
    }
}

/* Handler para la interrupción de TIM11 (blinky) */
void TIM1_TRG_COM_TIM11_IRQHandler(void){
    if (TIM11->SR & TIM_SR_UIF)
        TIM11->SR &= ~TIM_SR_UIF; // Baja la bandera (escribiendo 0) para que la interrupción se pueda repetir
	    blink_flag = 1; // La FSM lo convierte en EV_BLINK_TICK
}

/* Handler para la EXTI (botón) */
void EXTI0_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR0)
        EXTI->PR |= EXTI_PR_PR0; // Limpiar la bandera pendiente (escribiendo 1)
        button_flag = 1; // La FSM lo convierte en EV_BUTTON_PRESS
    
}

/* Handler para la interrupción de captura */
void TIM4_IRQHandler(void){
    if (TIM4->SR & TIM_SR_CC4IF) { // Pregunta si se alzó la bandera
        TIM4->SR &= ~TIM_SR_CC4IF; // Limpiar la bandera de interrupción
        
    }
}

       /*capture_1= TIM4->CNT;
         capture_2= TIM4->CNT;
         signal_period = capture_2 - capture_1; //periodo en us.
        */

