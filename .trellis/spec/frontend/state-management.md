# State Management

> How state is managed in this project.

---

## N/A — No Frontend

This is an STM32F407 embedded C firmware project. There is no frontend state management library.

State in the embedded context is:
- Hardware register values (managed by HAL)
- Global/peripheral state structs (`GPIO_InitTypeDef`, etc.)
- Static variables in `.c` files
- MCU registers (RCC, GPIO, NVIC)

See backend guidelines for C coding conventions.
