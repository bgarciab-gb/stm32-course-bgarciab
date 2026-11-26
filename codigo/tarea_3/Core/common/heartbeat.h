/* Core/common/heartbeat.h */
#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdbool.h>
#include <stdint.h>

#define HEARTBEAT_MIN_MS    50U
#define HEARTBEAT_MAX_MS  5000U

void     heartbeat_init(uint32_t toggle_ms); /* inicializa TIM3 para hacer parpadear blinky cada toggle_ms miliseg. */
bool     heartbeat_set_period(uint32_t toggle_ms);  /* permite modificar el periodo, utilizada por el comando "blink" en Stage 2 */
uint32_t heartbeat_get_period(void);

#endif