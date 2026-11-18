/**
 ******************************************************************************
 * @file           : main.c
 * @author         : Bleidy Carolina Garcia Blanco. bgarciab@unal.edu.co
 * @brief          : Control de LED RGB y señal PWM con encoder rotatorio,
 *                   medición de frecuencia por input capture y LED de estado
 *                   (blinky), organizado como una máquina de estados finitos
 *                   (FSM).
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
 * ========================== RELOJ ========================== *
 *
 * todos los timers reciben 16 MHz.
 * 
 *
 *  Timer | Bus  | PSC    | Tick   | ARR        | Uso
 *  ------|------|--------|--------|------------|-------------------------------
 *  TIM2  | APB1 | 0      | -      | 0xFFFFFFFF | Encoder (modo encoder 3)
 *  TIM3  | APB1 | 15     | 1 us   | 1999       | 4 canales PWM a 500 Hz
 *  TIM4  | APB1 | 15     | 1 us   | 0xFFFF     | Input capture CH4 (periodo)
 *  TIM10 | APB2 | 15     | 1 us   | 999        | Base de tiempo 1 ms (debounce)
 *  TIM11 | APB2 | 15999  | 1 ms   | 499        | Tick de 500 ms (blinky)
 *
 * ========================== PINES ========================== *
 *
 ** Capturar DATA y CLK del encoder:
 * PA0 --> TIM2_CH1 (AF01)  [CLK]
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
 * PH1 --> led de estado,enciende y apaga cada 500 ms por el tick de TIM11
 *
 ** Pulsador (SW):
 * PC0 --> EXTI0 (flanco de bajada, pull-up interno)
 *
 *
 * ===================== FUNCIONAMIENTO ===================== *
 *
 * 1. Encoder rotatorio (TIM2 en modo encoder): TIM2->CNT sube o baja con
 *    cada flanco de CLK y DATA (4 cuentas = 1 paso mecánico). Cada paso
 *    suma o resta 100 al CCR del canal seleccionado por el modo actual,
 *    es decir, un 5 % del periodo (100 / 2000).
 *
 * 2. Pulsador (EXTI0 en PC0): cada pulsación válida (con antirrebote por
 *    software de 30 ms usando TIM10) cambia el modo:
 *        ROJO -> VERDE -> AZUL -> TODOS -> VOLTAJE -> ROJO
 *
 * 3. PWM (TIM3, 500 Hz): tres canales manejan el LED RGB y el cuarto (PB0)
 *    genera una señal cuyo valor medio (tras un filtro RC) actúa como
 *    "DAC" de voltaje variable.
 *
 * 4. Medición de frecuencia (TIM4_CH4 en PB9): la ISR guarda el valor del
 *    contador en cada flanco de subida; la diferencia entre dos capturas
 *    consecutivas es el periodo en us, y la FSM calcula f = 1000000 / periodo.
 *    Rango de medición aprox.: 16 Hz (periodo < 65 536 us) hasta ~ cientos de kHz.
 *
 * 5. Blinky (TIM11 + PH1): cada 500 ms la ISR levanta una bandera y la FSM
 *    invierte el LED de estado (1 Hz de parpadeo), indicando que el
 *    programa principal no está bloqueado.
 *
 * Las ISR solo levantan banderas -> las interrupciones son cortas.
 *
 *
 * =========================== FSM =========================== *
 *
 *  Estado                   | Evento que lo activa  | Acción
 *  -------------------------+-----------------------+-------------------------
 *  STATE_IDLE               | -                     | Consulta get_next_event()
 *  STATE_UPDATE_PWM         | EV_ENCODER_CHANGED    | Lee pasos y ajusta CCRx
 *  STATE_CHANGE_MODE        | EV_BUTTON_PRESS       | Pasa al siguiente modo
 *  STATE_BLINK              | EV_BLINK_TICK         | Invierte PH1
 *  STATE_MEASURE_FREQUENCY  | EV_CAPTURE_READY      | frequency = 1000000/periodo
 *  STATE_ERROR              | estado fuera de rango | Vuelve a IDLE
 *
 *  Todos los estados de acción ejecutan su tarea una sola vez (al entrar,
 *  con state_entry_flag) y regresan inmediatamente a STATE_IDLE.
 *
 *  Prioridad de eventos en get_next_event():
 *      botón > encoder > blinky > captura
 *
 ***************************************************************
 */

#include <stdint.h>        // Tipos de ancho fijo: uint8_t, int32_t, etc.
#include "stm32f411xe.h"   // Definiciones de registros y máscaras del STM32F411xE
#include "stm32f4xx.h"     // Cabecera CMSIS genérica de la familia F4 (incluye core_cm4.h y NVIC)

//-----------------Máquina de estados finitos----------------------------------

// MODOS:
typedef enum { //Lista de modos posibles
	MODE_RED, //El encoder cambia el CCR2 (ancho del pulso para rojo)
	MODE_GREEN, //El encoder cambia el CCR4 (ancho del pulso para verde)
    MODE_BLUE, //El encoder cambia el CCR1 (ancho del pulso para azul)
    MODE_ALL, //El encoder cambia el CCR1,2,4 (de los tres pulsos)
    MODE_OTHER //El encoder cambia el CCR3 (ancho del pulso para el voltaje DAC)
} Mode_t; //Nombre del tipo: current_mode solo puede recibir valores de este enum

// ESTADOS: la condición en la que está el sistema en un instante dado
typedef enum { //Lista de estados posibles, cada nombre vale un entero (0, 1, 2...)
	STATE_IDLE, //Reposo: espera un evento sin hacer nada más (no bloquea)
	STATE_UPDATE_PWM, //Atiende el giro del encoder: suma o resta al CCR del PWM según el modo
	STATE_CHANGE_MODE, //Atiende el pulsador: cambia entre los 5 modos
	STATE_BLINK, //Atiende el tick del TIM11: cambia el estado del LED PH1
    STATE_MEASURE_FREQUENCY, //Atiende la detección de los flancos para medir periodo-->frecuencia
	STATE_ERROR, //Estado inválido (trampa de hardware): se devuelve hacia STATE_IDLE
	NUM_STATES //Centinela: cantidad de estados, sirve para validar el rango
} State_t; //Nombre del tipo: current_state solo puede recibir valores de este enum

// EVENTOS: provocan transiciones entre estados
typedef enum { //Lista de eventos posibles, cada nombre vale un entero (0, 1, 2...)
	EV_NONE, //No ha ocurrido nada
	EV_ENCODER_CHANGED, //El encoder avanzó o retrocedió al menos un paso
	EV_BUTTON_PRESS, //El pulsador fue presionado (flanco de bajada)
	EV_BLINK_TICK, //Pasaron 500 ms: toca invertir el LED de estado
    EV_CAPTURE_READY, //TIM4 midió un periodo nuevo de la señal en PB9
	NUM_EVENTS //Centinela: cantidad de eventos, sirve para validar el rango
} Event_t; //Nombre del tipo: current_event solo puede recibir valores de este enum


//-----------------Definición de variables----------------------------------

// Variable estado actual:
State_t current_state = STATE_IDLE; // Estado inicial en reposo
//Bandera para acceder a los estados:
uint8_t state_entry_flag = 1; // 1 = acabamos de entrar al estado (ejecutar la acción de entrada)
// Modo actual:
Mode_t current_mode = MODE_RED; // inicia modificando el duty para led rojo

volatile uint32_t time_ms = 0; // contador de milisegundos (lo incrementa la ISR de TIM10); volatile porque cambia dentro de una ISR

volatile uint8_t button_irq_flag = 0; // la ISR de EXTI0 la pone en 1 al detectar un flanco de bajada

uint8_t debounce_state = 0;  // estado de la mini-FSM de antirrebote (0,1,2,3)
uint32_t debounce_time = 0;  // instante (en ms) en que empezó la espera de 30 ms

volatile uint8_t button_flag = 0; //Bandera de pulsación ya validada (la levanta button_debounce)
volatile uint8_t blink_flag = 0; //Bandera que deja la ISR del TIM11

int32_t arr_value = 1999; //ARR que tiene el TIM3 (periodo = ARR + 1 us): (1999 + 1) us -> 500 Hz.
int32_t ccr_value_red = 0; //CCR --> ancho del pulso led rojo
int32_t ccr_value_green = 0; //CCR --> ancho del pulso led verde
int32_t ccr_value_blue = 0; //CCR --> ancho del pulso led azul
int32_t ccr_value_voltage = 0; //CCR --> ancho del pulso para generar señal de voltaje

// Captura de señal:
volatile uint16_t capture_1 = 0;     // valor de TIM4->CNT en el flanco anterior
volatile uint16_t capture_2 = 0;     // valor de TIM4->CNT en el flanco actual
volatile uint16_t signal_period = 0; // diferencia entre capturas = periodo en us

volatile uint8_t first_capture = 0;  // 0 = aún no hay una captura de referencia
volatile uint8_t capture_flag = 0;   // la ISR de TIM4 avisa que hay un periodo nuevo

volatile uint32_t frequency = 0;     // frecuencia medida en Hz (calculada en la FSM)


//-----------------Cabecera de funciones--------------------------------------

void button_debounce(void); // antirrebote del pulsador

// drivers FSM:
Event_t get_next_event(void);      // revisa banderas/periféricos y devuelve el evento pendiente
int32_t encoder_take_steps(void);  // lee pasos del encoder y reinicia el contador
void pwm_apply_steps(int32_t steps); // aplica los pasos al CCR del modo actual
void change_mode (void);           // avanza al siguiente modo
void blinky_on_off(void);          // invierte el LED PH1

// Inicialización
void init_gpio(void);           // relojes de puertos y configuración de pines
void init_timer2_encoder(void); // TIM2 en modo encoder
void init_timer3_PWM(void);     // TIM3 con 4 canales PWM
void init_timer4_capture(void); // TIM4 CH4 en input capture
void init_timer10(void); //Debounce (base de tiempo de 1 ms)
void init_timer11(void); //Blinky (tick de 500 ms)
void init_EXTI(void);           // interrupción externa del pulsador PC0


//--------------------------main-----------------------------------
int main(void)
{
    Event_t current_event = EV_NONE; // evento leído en cada pasada por IDLE
    init_gpio();            // configurar los pines (y relojes)
    init_EXTI();            // interrupción del pulsador
    init_timer3_PWM();      // salidas PWM
    init_timer2_encoder();  // lectura del encoder
    init_timer4_capture();  // medición de frecuencia
    init_timer10(); //Debounce
    init_timer11(); //Blinky


    while(1) { // lazo infinito
        button_debounce(); // se llama en cada vuelta para ejecutar el antirrebote

        if(current_state >= NUM_STATES){ // protección: si el estado se corrompió (valor fuera del enum)
            current_state = STATE_ERROR;  // se fuerza el estado de error
            state_entry_flag = 1;         // y se marca la entrada a ese estado
        }

        switch (current_state) { // ejecuta el código del estado actual

        case STATE_IDLE:
            // entrar
            if(state_entry_flag) {   // acción de entrada (aquí no hay nada que hacer)
                state_entry_flag =0; // se baja para no repetirla
            }

            // hacer:
            current_event= get_next_event(); // pregunta si ocurrió algo

            // Transiciones:
            if (current_event == EV_BUTTON_PRESS) {    // se presionó el botón
                current_state = STATE_CHANGE_MODE;     // ir a cambiar de modo
                state_entry_flag = 1;                  // marcar entrada al nuevo estado
            }

            else if (current_event == EV_ENCODER_CHANGED) { // se giró el encoder
                current_state = STATE_UPDATE_PWM;           // ir a actualizar el PWM
                state_entry_flag = 1;
            }
            else if (current_event == EV_BLINK_TICK) {      // pasaron 500 ms
                current_state = STATE_BLINK;                // ir a invertir el LED
                state_entry_flag = 1;
            }
            else if (current_event == EV_CAPTURE_READY){    // hay un periodo medido nuevo (captura)
                current_state = STATE_MEASURE_FREQUENCY;    // ir a calcular la frecuencia
                state_entry_flag = 1;
            }
            // si el evento es EV_NONE se queda en IDLE

            break;


        case STATE_UPDATE_PWM:
            //Entra:
            if(state_entry_flag){
                pwm_apply_steps(encoder_take_steps()); // lee los pasos del encoder y los aplica al CCR
                state_entry_flag = 0;                  // acción hecha
            }
            current_state = STATE_IDLE; // regresa a reposo
            state_entry_flag=1;         // marca la entrada a IDLE
            break;

        case STATE_CHANGE_MODE:
            //Entra:
            if(state_entry_flag){
                change_mode();          // pasa al siguiente modo
                state_entry_flag = 0;
            }
            //Vuelve al estado de reposo
            current_state = STATE_IDLE;
            state_entry_flag=1;
            break;
        case STATE_BLINK:
            //Entra:
            if(state_entry_flag){
                blinky_on_off();        // invierte el LED PH1
                state_entry_flag = 0;
            }
            current_state = STATE_IDLE; // regresa a reposo
            state_entry_flag=1;
            break;

        case STATE_MEASURE_FREQUENCY:

            if (state_entry_flag){
                if (signal_period != 0){                    // evita dividir entre cero
                frequency = 1000000UL / signal_period;      // tick de 1 us -> f[Hz] = 1 000 000 / periodo[us]
                }

                state_entry_flag = 0;
                }

            current_state = STATE_IDLE; // regresa a reposo
            state_entry_flag = 1;
            break;

        case STATE_ERROR:
        default:                         // cualquier valor inesperado cae aquí
            current_state = STATE_IDLE;  // recuperación: volver a reposo
            state_entry_flag=1;
            break;
        }

    }
    return 0; 
}

//===================== FUNCIONES DE FSM =====================//

/* Revisa las fuentes de eventos en orden de prioridad y devuelve UNO solo.
 * Las banderas se bajan al detectar el evento, para no procesar el mismo evento dos veces. */
Event_t get_next_event(void){
    if(button_flag==1){        // ¿hay una pulsación validada?
        button_flag=0;         // limpiar la bandera
        return EV_BUTTON_PRESS;
    }

    int32_t contador = (int32_t) TIM2->CNT; //valor del contador cuando se gira el encoder (con signo permite ver giros negativos: 0xFFFFFFFF -> -1)
    if ((contador >= 4) || (contador <= -4)) { //4 cuentas = 1 paso mecánico (modo encoder 3). Hay al menos un paso completo, horario o antihorario
        return EV_ENCODER_CHANGED; // no se reinicia aquí: lo hace encoder_take_steps()
    }

    if (blink_flag == 1) { // ¿TIM11 marcó otros 500 ms?
        blink_flag = 0;    // limpiar la bandera
        return EV_BLINK_TICK;
    }

    if (capture_flag == 1){ // ¿TIM4 midió un periodo nuevo?
    capture_flag = 0;       // limpiar la bandera
    return EV_CAPTURE_READY;
    }

    return EV_NONE; // no pasó nada
}

/* Convierte las cuentas de TIM2 en pasos mecánicos y reinicia el contador. */
int32_t encoder_take_steps(void){
    int32_t steps = (int32_t) TIM2->CNT / 4; //4 cuentas = 1 paso (signo + = un sentido, - = el otro)
    TIM2->CNT = 0; //Se reinicia CNT para medir el siguiente giro desde cero
    return steps;
}

/* Suma (steps * 100) al CCR del modo actual, limita el valor a [0, ARR+1]
 * y escribe los cuatro CCR del TIM3. */
void pwm_apply_steps(int32_t steps){

    if(current_mode == MODE_RED){              // modo rojo
        ccr_value_red += steps *100;           // cada paso = 100 us = 5 % del periodo
    }
    else if (current_mode== MODE_GREEN) {      // modo verde
        ccr_value_green += steps *100;
    }
    else if (current_mode== MODE_BLUE) {       // modo azul
        ccr_value_blue += steps *100;
    }
    else if (current_mode== MODE_ALL) {        // modo todos: los tres colores a la vez
        ccr_value_red += steps *100;
        ccr_value_green += steps *100;
        ccr_value_blue += steps *100;
    }
    else if (current_mode== MODE_OTHER) {      // modo voltaje (PB0)
        ccr_value_voltage += steps *100;
    }

    // Saturación: el duty no puede ser negativo ni mayor al 100 %
    // (en PWM modo 1, CCR > ARR deja la salida siempre en alto = 100 %)
    if (ccr_value_red < 0) ccr_value_red = 0;
    if (ccr_value_red > arr_value + 1) ccr_value_red = arr_value + 1;

    if (ccr_value_green < 0) ccr_value_green = 0;
    if (ccr_value_green > arr_value + 1) ccr_value_green = arr_value + 1;

    if (ccr_value_blue < 0) ccr_value_blue = 0;
    if (ccr_value_blue > arr_value + 1) ccr_value_blue = arr_value + 1;

    if (ccr_value_voltage < 0) ccr_value_voltage = 0;
    if (ccr_value_voltage > arr_value + 1) ccr_value_voltage = arr_value + 1;


    // Asignar el valor a los registros (con preload: se aplican en el próximo evento de update)
    TIM3->CCR2 = ccr_value_red;     // CH2 -> PC7 (rojo)
    TIM3->CCR4 = ccr_value_green;   // CH4 -> PC9 (verde)
    TIM3->CCR1 = ccr_value_blue;    // CH1 -> PC6 (azul)
    TIM3->CCR3 = ccr_value_voltage; // CH3 -> PB0 (voltaje)
}

/* Avanza al siguiente modo de forma circular. */
void change_mode (void){
    if (current_mode==MODE_RED) {
        current_mode=MODE_GREEN;   // rojo -> verde
    }
    else if (current_mode==MODE_GREEN) {
        current_mode=MODE_BLUE;    // verde -> azul
    }
    else if (current_mode==MODE_BLUE) {
        current_mode=MODE_ALL;     // azul -> todos
    }
    else if (current_mode==MODE_ALL) {
        current_mode=MODE_OTHER;   // todos -> voltaje
    }
    else if (current_mode==MODE_OTHER) {
        current_mode=MODE_RED;     // voltaje -> rojo (vuelve a empezar)
    }

}

/* Antirrebote (mini-FSM de 4 estados):
 *  0: espera el flanco que reporta la EXTI
 *  1: espera 30 ms y confirma que el pin sigue en bajo (pulsado)
 *  2: espera a que el botón se suelte
 *  3: espera 30 ms con el botón suelto para confirmar la liberación
 * El pulsador es activo en bajo: IDR0 = 0 -> presionado. */
void button_debounce(void){

    switch (debounce_state){

        case 0: // Esperar pulsación
            if (button_irq_flag){          // la EXTI vio un flanco de bajada

                button_irq_flag = 0;       // bajar bandera
                debounce_time = time_ms;   // se guarda el instante
                debounce_state = 1;        // pasa a confirmar
            }
            break;

        case 1: // Confirmar pulsación
            if ((uint32_t)(time_ms - debounce_time) >= 30){ // pasaron 30 ms

                if (!(GPIOC->IDR & GPIO_IDR_ID0)){ // PC0 sigue en 0: pulsación real
                    button_flag = 1;               // avisa a la FSM principal
                    debounce_state = 2;            // ahora espera que se deje de presionar el botón
                }
                else {
                    debounce_state = 0;            // era ruido/rebote: se descarta
                }
            }
            break;

        case 2: // Esperar liberación
            button_irq_flag = 0;            // ignora flancos (rebotes) mientras está presionado

            if (GPIOC->IDR & GPIO_IDR_ID0){ // PC0 volvió a 1: se soltó

                debounce_time = time_ms;    // inicia la espera de confirmación
                debounce_state = 3;
            }
            break;

        case 3: // Confirmar liberación
            button_irq_flag = 0;            // sigue ignorando rebotes

            if (!(GPIOC->IDR & GPIO_IDR_ID0)){ // volvió a 0: era un rebote al soltar
                debounce_state = 2;            // regresa a esperar liberación
            }
            else if ((uint32_t)(time_ms - debounce_time) >= 30){ // 30 ms estable en 1
                debounce_state = 0;            // liberación confirmada: listo para otra pulsación
            }
            break;
    }
}

// Cambiar el estado del led PH1:
void blinky_on_off (void){
	GPIOH->ODR ^= GPIO_ODR_OD1; //XOR con 1: invierte el bit sin tocar los otros pines
}

//===================== FUNCIONES DE INICIALIZACIÓN =====================//

void init_gpio(void){

    // Habilitar el reloj para los puertos GPIO (A, B, C y H)

    RCC->AHB1ENR &= ~(RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOHEN); // Limpiar bits de habilitación 
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // Habilitar el reloj para GPIOA
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; // Habilitar el reloj para GPIOC
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; // Habilitar el reloj para GPIOB
    RCC ->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; // Habilitar el reloj para GPIOH

    // Configurando GPIOH - pin 1 LED blinky, breadboard

    GPIOH->MODER    &= ~(GPIO_MODER_MODE1);     //Limpiar MODER1 (00)
    GPIOH->MODER    |= (GPIO_MODER_MODE1_0);    //01: PH1 como salida
    GPIOH->OTYPER   &= ~(GPIO_OTYPER_OT1);      // 0: salida push-pull
    GPIOH->OSPEEDR  &= ~(GPIO_OSPEEDR_OSPEED1); // 00: velocidad baja
    GPIOH->PUPDR    &= ~(GPIO_PUPDR_PUPD1);     // 00: sin resistencia pull-up ni pull-down
    GPIOH->ODR      &= ~(GPIO_ODR_OD1);         // Inicializar el pin PH1 en 0 lógico (led apagado)

    // Configurando GPIOC pin 0 (EXTI pulsador):

    GPIOC->MODER    &= ~(GPIO_MODER_MODE0);   // 00: PC0 como entrada digital
    GPIOC->PUPDR    &= ~(GPIO_PUPDR_PUPD0);   // Limpiar la resistencia de pull up / pull down
    GPIOC->PUPDR |= GPIO_PUPDR_PUPD0_0;       // 01: pull-up interno (en reposo = 1, presionado = 0)

    /* Pines salida PWM */

    // Configurar PB0 como función alternativa AF02 (TIM3_CH3)
    GPIOB->MODER &= ~GPIO_MODER_MODE0;   //Limpiar (00)
    GPIOB->MODER |= GPIO_MODER_MODE0_1;  //(10) Función alternativa
    GPIOB->AFR[0] &= ~GPIO_AFRL_AFRL0;   //Limpiar AFRL0 (AFR[0] = AFRL, pines 0..7)
    GPIOB->AFR[0] |= GPIO_AFRL_AFRL0_1;  //(0010 AF02)

    // Configurar PC6, PC7, PC9 como función alternativa AF02 (TIM3_CH1, CH2, CH4)
    GPIOC->MODER &= ~(GPIO_MODER_MODE6|GPIO_MODER_MODE7|GPIO_MODER_MODE9);       //Limpiar (00)
    GPIOC->MODER |= (GPIO_MODER_MODE6_1|GPIO_MODER_MODE7_1|GPIO_MODER_MODE9_1);  //(10) Función alternativa
    GPIOC->AFR[0] &= ~(GPIO_AFRL_AFRL6|GPIO_AFRL_AFRL7);       //Limpiar AF de PC6 y PC7 (en AFRL)
    GPIOC->AFR[1] &= ~(GPIO_AFRH_AFRH1 );                      //Limpiar AF de PC9 (AFR[1] = AFRH; campo 1 = pin 8+1 = 9)
    GPIOC->AFR[0] |= (GPIO_AFRL_AFRL6_1|GPIO_AFRL_AFRL7_1);    //(0010 AF02) para PC6 y PC7
    GPIOC->AFR[1] |= GPIO_AFRH_AFRH1_1;                        //(0010 AF02) para PC9

    /* Pines para capture/compare */

    // Encoder (CLK: PA0 AF01) (DT: PA1 AF01) -> TIM2_CH1 y TIM2_CH2
    GPIOA->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1);     //Limpiar (00)
    GPIOA->MODER |= (GPIO_MODER_MODE0_1 | GPIO_MODER_MODE1_1);  //(10) Función alternativa
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFRL0 | GPIO_AFRL_AFRL1);      //Limpiamos
    GPIOA->AFR[0] |= (GPIO_AFRL_AFRL0_0 | GPIO_AFRL_AFRL1_0);   //(0001 AF01)

    // Capturar frecuencia (PB9 AF02) -> TIM4_CH4
    GPIOB->MODER &= ~GPIO_MODER_MODE9;   //Limpiar (00)
    GPIOB->MODER |= GPIO_MODER_MODE9_1;  //(10) Función alternativa
    GPIOB->AFR[1] &= ~GPIO_AFRH_AFRH1;   //Limpiar (campo de PB9 en AFRH)
    GPIOB->AFR[1] |= GPIO_AFRH_AFRH1_1;  //(0010 AF02)

}

void init_EXTI(void){
    RCC->APB2ENR &= ~(RCC_APB2ENR_SYSCFGEN); // Deshabilitar el reloj para SYSCFG (limpieza)
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;    // Habilitar el reloj para SYSCFG 

    SYSCFG->EXTICR[0] &= ~SYSCFG_EXTICR1_EXTI0;    // Limpiar el selector de puerto de la línea EXTI0
    SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PC;  // La línea EXTI0 viene del puerto C (PC0)

    EXTI->RTSR &= ~EXTI_RTSR_TR0; // desactivar interrupción por flanco de subida
    EXTI->FTSR |= EXTI_FTSR_TR0;  // interrupción por flanco de bajada (al presionar)

    // EXTI->PR = EXTI_PR_PR0; // Limpiar la bandera pendiente (se limpia escribiendo 1)
    EXTI->IMR |= EXTI_IMR_MR0; // 'Desenmascarar' EXTI0 (habilitar la interrupción)

    __NVIC_EnableIRQ(EXTI0_IRQn);   // Habilitar en el NVIC la interrupción proveniente de EXTI0 (PC0)
}

void init_timer3_PWM(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; //señal de reloj del TIM3 (bus APB1)

    TIM3->PSC = 16-1;            //16 MHz / 16 = 1 MHz -> tick de 1 us
	TIM3->ARR = arr_value;       //Periodo: (1999 + 1) us = 2 ms -> 500 Hz
	TIM3->EGR |= TIM_EGR_UG;     //Fuerza un update para cargar PSC y ARR en sus registros sombra
	TIM3->CR1 |= TIM_CR1_ARPE;   //Preload del ARR: cambios de ARR se aplican al final del periodo

    //Canal 1 LED AZUL:
    TIM3->CCMR1 |= (TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2); //OC1M = 110: PWM modo 1 (alto mientras CNT < CCR1)
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE; //CCR1 con preload
    TIM3->CCER |= TIM_CCER_CC1E;    //Habilita la salida del canal 1 al pin
    TIM3->CCR1 = ccr_value_blue;    //Ancho del pulso inicial

    //Canal 2 LED ROJO:
    TIM3->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2); //OC2M = 110: PWM modo 1
    TIM3->CCMR1 |= TIM_CCMR1_OC2PE; //CCR2 con preload
    TIM3->CCER |= TIM_CCER_CC2E;    //Habilita la salida del canal 2 al pin
    TIM3->CCR2 = ccr_value_red;     //Ancho del pulso inicial

    //Canal 3 SEÑAL DE VOLTAJE:
    TIM3->CCMR2 |= (TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2); //OC3M = 110: PWM modo 1 (canales 3 y 4 están en CCMR2)
    TIM3->CCMR2 |= TIM_CCMR2_OC3PE; //CCR3 con preload
    TIM3->CCER |= TIM_CCER_CC3E;    //Habilita la salida del canal 3 al pin
    TIM3->CCR3 = ccr_value_voltage; //Ancho del pulso inicial

    //Canal 4 LED VERDE:
    TIM3->CCMR2 |= (TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2); //OC4M = 110: PWM modo 1
    TIM3->CCMR2 |= TIM_CCMR2_OC4PE; //CCR4 con preload
    TIM3->CCER |= TIM_CCER_CC4E;    //Habilita la salida del canal 4 al pin
    TIM3->CCR4 = ccr_value_green;   //Ancho del pulso inicial

    //Arrancar el contador del timer
    TIM3->CR1 |= TIM_CR1_CEN; //siempre de último, cuando todo está configurado
}

void init_timer2_encoder(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; //Reloj del TIM2 (bus APB1)

    TIM2->ARR = 0xFFFFFFFF;      //TIM2 es de 32 bit: no se desborda en uso normal
    TIM2->CR1 |= TIM_CR1_ARPE;   //Preload del ARR

    TIM2->CCMR1 |= (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0); //01: CH1 y CH2 como entrada, mapeados a TI1 y TI2
    TIM2->CCMR1 |= (TIM_CCMR1_IC1F | TIM_CCMR1_IC2F);     //1111: filtro  máximo, rechaza el rebote del encoder
    TIM2->CCER |= (TIM_CCER_CC1E | TIM_CCER_CC2E);        //Habilita los dos canales de entrada

    TIM2->SMCR |= (TIM_SMCR_SMS_0 | TIM_SMCR_SMS_1); //SMS = 011: modo encoder 3, cuenta flancos de TI1 y TI2 (x4)
    TIM2->CR1 |= TIM_CR1_CEN; //Arranca el contador
}

void init_timer4_capture(void){
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; //Reloj del TIM4 (bus APB1)

    TIM4->ARR = 0xFFFF;          //máximo para un timer de 16 bits: periodo medible hasta 65,5 ms
    TIM4->PSC = 16-1;            //16 MHz / 16 = 1 MHz -> cuenta en us
    TIM4->CR1 |= TIM_CR1_ARPE;   //Preload del ARR
    TIM4->EGR |= TIM_EGR_UG;     // Forzar que se cargue el PSC antes de empezar a contar
    TIM4->SR &= ~TIM_SR_UIF;     //El UG levanta UIF: se baja para iniciar limpio

    // Canal 4:
    TIM4->CCMR2 |= (TIM_CCMR2_CC4S_0); //CC4S = 01: CH4 como entrada, mapeado a TI4 (PB9)

    TIM4->CCER &= ~(TIM_CCER_CC4P|TIM_CCER_CC4NP); //CC4NP:CC4P = 00 -> captura en flanco de subida
    TIM4->CCER |= (TIM_CCER_CC4E);                 //Habilita la captura del CH4

    TIM4->DIER |= (TIM_DIER_CC4IE); //Interrupción cada vez que se captura un flanco


    __NVIC_EnableIRQ(TIM4_IRQn); //Habilita la interrupción de TIM4 en el NVIC
    TIM4->CR1 |= TIM_CR1_CEN;    //Arranca el contador

}

void init_timer10(void){

    RCC->APB2ENR &= ~(RCC_APB2ENR_TIM10EN);    //limpiar 
    RCC->APB2ENR |= (RCC_APB2ENR_TIM10EN_Msk); // Habilitar señal de reloj para TIM10 (bus APB2)

    TIM10->PSC = 16-1;   // 16 MHz/16 = 1 MHz --> tick de 1 us
	TIM10->ARR = 1000-1;    // 1000 us --> desborda cada 1 ms
	TIM10->CR1 |= TIM_CR1_ARPE; //Habilitar la precarga del auto-reload
	TIM10->EGR |= TIM_EGR_UG;   //Update manual para cargar PSC y ARR
	TIM10->SR &= ~TIM_SR_UIF; //Update generation levanta UIF: se baja para que no cuente como un tick falso
	TIM10->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización (desborde)
	__NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn ); //Habilita la interrupción en el NVIC

	TIM10->CR1 |= TIM_CR1_CEN; //Arranca el contador
}

void init_timer11(void){

    RCC->APB2ENR &= ~(RCC_APB2ENR_TIM11EN);    //limpiar
    RCC->APB2ENR |= (RCC_APB2ENR_TIM11EN_Msk); // Habilitar señal de reloj para TIM11 (bus APB2)

    TIM11->PSC = 16000-1;   // 16 MHz/16000 = 1 kHz --> tick de 1 ms
	TIM11->ARR = 500-1;     // 500 ticks --> desborda cada 500 ms
	TIM11->CR1 |= TIM_CR1_ARPE; //Habilitar la precarga del auto-reload
	TIM11->EGR |= TIM_EGR_UG;   //Update manual para cargar PSC y ARR
	TIM11->SR &= ~TIM_SR_UIF; //Update generation levanta UIF: se baja para que no cuente como un tick falso
	TIM11->DIER |= TIM_DIER_UIE; //Habilita la interrupción por actualización
	__NVIC_EnableIRQ(TIM1_TRG_COM_TIM11_IRQn); //Habilita la interrupción en el NVIC

	TIM11->CR1 |= TIM_CR1_CEN; //Arranca el contador
}


//===================== HANDLERS INTERRUPCIONES =====================//

/* Handler para la interrupción de TIM10 (debounce) */
void TIM1_UP_TIM10_IRQHandler(void){
    if (TIM10->SR & TIM_SR_UIF) { // Pregunta si fue TIM10 quien desbordó
        TIM10->SR &= ~TIM_SR_UIF; // Limpiar la bandera (escribiendo 0) para no reentrar
        time_ms++;                // Aumenta un milisegundo
    }
}

/* Handler para la interrupción de TIM11 (blinky) */
void TIM1_TRG_COM_TIM11_IRQHandler(void){
    if (TIM11->SR & TIM_SR_UIF){  // ¿desbordó TIM11?
        TIM11->SR &= ~TIM_SR_UIF; // Baja la bandera (escribiendo 0) para que la interrupción se pueda repetir
	    blink_flag = 1; // La FSM lo convierte en EV_BLINK_TICK

    }

}

/* Handler para la EXTI (botón) */
void EXTI0_IRQHandler(void){
    if (EXTI->PR & EXTI_PR_PR0){ // ¿la interrupción pendiente es de la línea 0?
        EXTI->PR = EXTI_PR_PR0; // Limpiar la bandera pendiente (escribiendo 1)
        button_irq_flag=1;       // avisa a button_debounce() que hubo un flanco
        }

    }


/* Handler para la interrupción de captura */
void TIM4_IRQHandler(void){

    if (TIM4->SR & TIM_SR_CC4IF){ // ¿hubo una captura en el canal 4?

        uint16_t capture = TIM4->CCR4; // lee el valor capturado

        if (first_capture == 0){   // primer flanco: solo sirve de referencia
            capture_1 = capture;
            first_capture = 1;
        }
        else {
            capture_2 = capture;                                 // flanco actual
            signal_period = (uint16_t)(capture_2 - capture_1);   // periodo en us

            capture_1 = capture_2;  // el flanco actual es la referencia del siguiente
            capture_flag = 1;       // avisa a la FSM: EV_CAPTURE_READY
        }
    }
}