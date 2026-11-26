/* Core/common/app.h — En este archivo declaramos las dos funciones que todas las aplicaciones deben proporcionar */
#ifndef APP_H
#define APP_H

void app_init(void);   /* Se llama una vez, después de que clock y heartbeat están corriendo...*/
void app_loop(void);   /* Se llama siempre desde el while(1) en main() */

#endif