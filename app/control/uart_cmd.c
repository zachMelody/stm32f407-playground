#include "control/uart_cmd.h"

#include "control/pwm_control.h"

#include "usart.h"

#include <stdio.h>

#define DMA_BUF_SIZE 64u
#define CMD_LINE_MAX 16u

static volatile uint8_t s_rx_buf[DMA_BUF_SIZE];
static uint8_t s_tx_buf[DMA_BUF_SIZE];
static uint32_t s_rx_last_ndtr;
static volatile uint8_t s_tx_busy;
static uint8_t s_pr_buf[DMA_BUF_SIZE];
static int s_pr_buf_cnt;
static char s_cmd_line[CMD_LINE_MAX];
static uint8_t s_cmd_len;

static void Cmd_Process(const char *line)
{
  uint32_t val = 0;
  int digits = 0;

  for (const char *p = line; *p != '\0'; p++) {
    if (*p < '0' || *p > '9') {
      printf("[ERR] expect integer 0..95\r\n");
      return;
    }
    val = val * 10u + (uint32_t)(*p - '0');
    if (++digits > 3) {
      printf("[ERR] expect integer 0..95\r\n");
      return;
    }
  }

  if (digits == 0) {
    printf("[ERR] expect integer 0..95\r\n");
    return;
  }

  if (val > 95u) {
    printf("[ERR] duty must be 0..95 (EG2132 bootstrap)\r\n");
    return;
  }

  PwmControl_SetDutyX10((uint16_t)(val * 10u));
  printf("[OK] duty=%lu%%\r\n", (unsigned long)val);
}

static void Cmd_FeedByte(uint8_t ch)
{
  if (ch == '\r' || ch == '\n') {
    if (s_cmd_len > 0u) {
      s_cmd_line[s_cmd_len] = '\0';
      Cmd_Process(s_cmd_line);
      s_cmd_len = 0u;
    }
    return;
  }

  if (s_cmd_len < CMD_LINE_MAX - 1u) {
    s_cmd_line[s_cmd_len++] = (char)ch;
  } else {
    s_cmd_len = 0u;
    printf("[ERR] line too long (max %d)\r\n", (int)CMD_LINE_MAX - 1);
  }
}

int __io_putchar(int ch)
{
  if (s_pr_buf_cnt == 0) {
    while (s_tx_busy) {
    }
  }

  s_pr_buf[s_pr_buf_cnt++] = (uint8_t)ch;
  if (s_pr_buf_cnt >= (int)DMA_BUF_SIZE || ch == '\n') {
    while (s_tx_busy) {
    }
    __HAL_DMA_DISABLE(huart1.hdmatx);
    huart1.hdmatx->State = HAL_DMA_STATE_READY;
    huart1.gState = HAL_UART_STATE_READY;
    s_tx_busy = 1u;
    if (HAL_UART_Transmit_DMA(&huart1, s_pr_buf, (uint16_t)s_pr_buf_cnt) != HAL_OK) {
      s_tx_busy = 0u;
    }
    s_pr_buf_cnt = 0;
  }

  return ch;
}

void UartCmd_InitState(void)
{
  s_rx_last_ndtr = DMA_BUF_SIZE;
  s_tx_busy = 0u;
  s_pr_buf_cnt = 0;
  s_cmd_len = 0u;
}

uint8_t *UartCmd_GetRxBuffer(void)
{
  return (uint8_t *)s_rx_buf;
}

uint32_t UartCmd_GetRxBufferSize(void)
{
  return DMA_BUF_SIZE;
}

void UartCmd_EchoCheck(void)
{
  uint32_t ndtr;
  uint32_t bytes;

  if (s_tx_busy) {
    return;
  }

  ndtr = __HAL_DMA_GET_COUNTER(huart1.hdmarx);
  bytes = (s_rx_last_ndtr - ndtr + DMA_BUF_SIZE) % DMA_BUF_SIZE;

  if (bytes > 0u) {
    uint32_t pos = (DMA_BUF_SIZE - s_rx_last_ndtr) % DMA_BUF_SIZE;
    for (uint32_t i = 0; i < bytes; i++) {
      uint8_t ch = s_rx_buf[pos];
      s_tx_buf[i] = ch;
      Cmd_FeedByte(ch);
      pos = (pos + 1u) % DMA_BUF_SIZE;
    }

    s_rx_last_ndtr = ndtr;
    __HAL_DMA_DISABLE(huart1.hdmatx);
    huart1.hdmatx->State = HAL_DMA_STATE_READY;
    huart1.gState = HAL_UART_STATE_READY;
    s_tx_busy = 1u;
    if (HAL_UART_Transmit_DMA(&huart1, s_tx_buf, (uint16_t)bytes) != HAL_OK) {
      s_tx_busy = 0u;
    }
  }
}

void UartCmd_OnTxComplete(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    s_tx_busy = 0u;
  }
}
