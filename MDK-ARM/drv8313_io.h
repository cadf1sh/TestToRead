#ifndef DRV8313_IO_H
#define DRV8313_IO_H

#include "app_config.h"

typedef enum
{
    DRV8313_OUT1 = 0,
    DRV8313_OUT2,
    DRV8313_OUT3
} drv8313_output_t;

void DRV8313_IO_Init(void);
void DRV8313_IO_SetEnable(drv8313_output_t output, GPIO_PinState state);
void DRV8313_IO_SetInput(drv8313_output_t output, GPIO_PinState state);
void DRV8313_IO_SetInputDuty(drv8313_output_t output, uint16_t duty_0_to_1000);
uint16_t DRV8313_IO_GetInputDuty(drv8313_output_t output);
void DRV8313_IO_AllOutputsOff(void);
void DRV8313_IO_SetNSleep(GPIO_PinState state);
void DRV8313_IO_SetNReset(GPIO_PinState state);
GPIO_PinState DRV8313_IO_ReadNFault(void);

#endif
