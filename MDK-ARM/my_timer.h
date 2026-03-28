#ifndef MY_TIMER_H
#define MY_TIMER_H

#include "app_config.h"

typedef void (*my_timer_callback_t)(void);

void my_Timer10ms_Init(void);
void my_Timer10ms_Start(void);
void my_Timer10ms_RegisterCallback(my_timer_callback_t callback);

#endif
