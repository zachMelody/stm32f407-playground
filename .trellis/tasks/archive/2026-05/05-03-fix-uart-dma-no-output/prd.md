# Fix UART DMA no output

## Goal
Restore USART1 DMA transmit output in the STM32F407 firmware so `printf()` and DMA echo can continuously send data.

## Requirements
- Keep the existing `USART1 + DMA2_Stream5/7` peripheral allocation.
- Preserve the current DMA echo and `printf` buffering flow in `Core/Src/main.c`.
- Fix the missing interrupt chain required by HAL UART DMA transmit in normal mode.
- Keep changes inside the existing CubeMX-generated file structure and user-edit-safe locations.

## Acceptance Criteria
- [ ] `USART1_IRQn` is enabled during USART1 MSP init.
- [ ] `USART1_IRQHandler()` exists and calls `HAL_UART_IRQHandler(&huart1)`.
- [ ] `HAL_UART_TxCpltCallback()` can be reached after DMA normal-mode transmit completion.
- [ ] No unrelated refactor is introduced.

## Technical Notes
- HAL UART DMA TX in normal mode completes in two stages: DMA stream transfer complete, then USART transmit-complete interrupt for the final byte.
- If the USART global IRQ is missing, `huart->gState` and the user `tx_busy` flag do not clear, which stalls subsequent output.
- Manual validation target: serial terminal at `115200 8N1`, observe startup banner and repeated echo/log output.
