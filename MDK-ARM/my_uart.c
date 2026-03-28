#include "my_uart.h"
#include <stdio.h>
#include <string.h>

int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;
    (void)f;
    my_Uart_SendBytes(&c, 1);
    return ch;
}

static UART_HandleTypeDef huart2;

void my_Uart_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    /* PA2 -> USART2_TX, PA3 -> USART2_RX */
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK) {
        while (1) {
            printf("uart init error\r\n");
            HAL_Delay(200);
        }
    }
}

HAL_StatusTypeDef my_Uart_SendBytes(const uint8_t *data, uint16_t len)
{
    return HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100);
}

HAL_StatusTypeDef my_Uart_SendString(const char *str)
{
    return my_Uart_SendBytes((const uint8_t *)str, (uint16_t)strlen(str));
}
