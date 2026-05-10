#include "control/uart_cmd.h"

#include "control/temperature_control.h"

#include "usart.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

static char *Cmd_SkipSpaces(char *text)
{
  while (*text == ' ' || *text == '\t') {
    text++;
  }
  return text;
}

static void Cmd_TrimTrailing(char *text)
{
  size_t len = strlen(text);

  while (len > 0u && (text[len - 1u] == ' ' || text[len - 1u] == '\t')) {
    text[--len] = '\0';
  }
}

static void Cmd_ToLowerAscii(char *text)
{
  while (*text != '\0') {
    *text = (char)tolower((unsigned char)*text);
    text++;
  }
}

static uint8_t Cmd_ParseUint(const char *text, uint32_t *value)
{
  uint32_t parsed = 0u;
  int digits = 0;

  if (text == NULL || *text == '\0') {
    return 0u;
  }

  for (const char *p = text; *p != '\0'; p++) {
    if (*p < '0' || *p > '9') {
      return 0u;
    }
    parsed = (parsed * 10u) + (uint32_t)(*p - '0');
    if (++digits > 4) {
      return 0u;
    }
  }

  if (value != NULL) {
    *value = parsed;
  }
  return (digits > 0) ? 1u : 0u;
}

void UartCmd_PrintStatus(void)
{
  temperature_control_snapshot_t ctrl_snapshot;
  uint32_t temp_abs_x10;
  uint32_t set_abs_x10;

  TemperatureControl_GetSnapshot(&ctrl_snapshot);
  set_abs_x10 = (ctrl_snapshot.setpoint_c_x10 < 0) ? (uint32_t)(-ctrl_snapshot.setpoint_c_x10) :
                                                     (uint32_t)ctrl_snapshot.setpoint_c_x10;

  if (ctrl_snapshot.has_temperature != 0u) {
    temp_abs_x10 = (ctrl_snapshot.measured_temp_c_x10 < 0) ? (uint32_t)(-ctrl_snapshot.measured_temp_c_x10) :
                                                             (uint32_t)ctrl_snapshot.measured_temp_c_x10;
    printf("[CTRL] mode=%s set=%s%lu.%01luC tmp=%s%lu.%01luC pwm=%u.%u%% tc=%luuV\r\n",
           TemperatureControl_ModeName(ctrl_snapshot.mode),
           (ctrl_snapshot.setpoint_c_x10 < 0) ? "-" : "",
           (unsigned long)(set_abs_x10 / 10u),
           (unsigned long)(set_abs_x10 % 10u),
           (ctrl_snapshot.measured_temp_c_x10 < 0) ? "-" : "",
           (unsigned long)(temp_abs_x10 / 10u),
           (unsigned long)(temp_abs_x10 % 10u),
           (unsigned)(ctrl_snapshot.applied_duty_x10 / 10u),
           (unsigned)(ctrl_snapshot.applied_duty_x10 % 10u),
           (unsigned long)ctrl_snapshot.tc_uv);
  } else {
    printf("[CTRL] mode=%s set=%s%lu.%01luC tmp=pending pwm=%u.%u%%\r\n",
           TemperatureControl_ModeName(ctrl_snapshot.mode),
           (ctrl_snapshot.setpoint_c_x10 < 0) ? "-" : "",
           (unsigned long)(set_abs_x10 / 10u),
           (unsigned long)(set_abs_x10 % 10u),
           (unsigned)(ctrl_snapshot.applied_duty_x10 / 10u),
           (unsigned)(ctrl_snapshot.applied_duty_x10 % 10u));
  }
}

static void Cmd_Process(const char *line)
{
  char buf[CMD_LINE_MAX];
  char *cmd;
  char *arg;
  uint32_t value;

  if (line == NULL) {
    return;
  }

  strncpy(buf, line, sizeof(buf) - 1u);
  buf[sizeof(buf) - 1u] = '\0';
  Cmd_TrimTrailing(buf);
  Cmd_ToLowerAscii(buf);

  cmd = Cmd_SkipSpaces(buf);
  if (*cmd == '\0') {
    printf("[ERR] expect: auto <C> | man <pct> | off | stat\r\n");
    return;
  }

  arg = cmd;
  while (*arg != '\0' && *arg != ' ' && *arg != '\t') {
    arg++;
  }
  if (*arg != '\0') {
    *arg++ = '\0';
    arg = Cmd_SkipSpaces(arg);
  }

  if (strcmp(cmd, "off") == 0) {
    if (*arg != '\0') {
      printf("[ERR] usage: off\r\n");
      return;
    }
    TemperatureControl_SetOff();
    printf("[OK] mode=OFF\r\n");
    UartCmd_PrintStatus();
    return;
  }

  if (strcmp(cmd, "stat") == 0) {
    if (*arg != '\0') {
      printf("[ERR] usage: stat\r\n");
      return;
    }
    UartCmd_PrintStatus();
    return;
  }

  if (strcmp(cmd, "auto") == 0) {
    if (!Cmd_ParseUint(arg, &value)) {
      printf("[ERR] usage: auto <tempC>\r\n");
      return;
    }
    if (!TemperatureControl_SetAutoSetpointCX10((int32_t)value * 10)) {
      printf("[ERR] auto setpoint must be %ld..%ldC\r\n",
             (long)(TEMP_CTRL_SETPOINT_MIN_C_X10 / 10),
             (long)(TEMP_CTRL_SETPOINT_MAX_C_X10 / 10));
      return;
    }
    printf("[OK] mode=AUTO set=%luC\r\n", (unsigned long)value);
    UartCmd_PrintStatus();
    return;
  }

  if (strcmp(cmd, "man") == 0) {
    if (!Cmd_ParseUint(arg, &value)) {
      printf("[ERR] usage: man <duty%%>\r\n");
      return;
    }
    if (value > 95u || !TemperatureControl_SetManualDutyX10((uint16_t)(value * 10u))) {
      printf("[ERR] manual duty must be 0..95 (EG2132 bootstrap)\r\n");
      return;
    }
    printf("[OK] mode=MAN duty=%lu%%\r\n", (unsigned long)value);
    UartCmd_PrintStatus();
    return;
  }

  printf("[ERR] unknown cmd: %s\r\n", cmd);
  printf("[ERR] expect: auto <C> | man <pct> | off | stat\r\n");
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
