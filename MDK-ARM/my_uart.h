#ifndef MY_UART_H
#define MY_UART_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

void my_Uart_Init(void);
HAL_StatusTypeDef my_Uart_SendBytes(const uint8_t *data, uint16_t len);
HAL_StatusTypeDef my_Uart_SendString(const char *str);

#endif

