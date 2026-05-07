#ifndef __UART_CMD_H__
#define __UART_CMD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

void UartCmd_InitState(void);
uint8_t *UartCmd_GetRxBuffer(void);
uint32_t UartCmd_GetRxBufferSize(void);
void UartCmd_EchoCheck(void);
void UartCmd_OnTxComplete(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __UART_CMD_H__ */
