# Type Safety

> Type safety patterns in this project.

---

## N/A — No Frontend

This is an STM32F407 embedded C firmware project. There is no TypeScript or frontend type system.

C type safety conventions:
- Use `stdint.h` types (`uint8_t`, `uint32_t`, etc.) — already used by HAL
- Use HAL typedefs (`GPIO_InitTypeDef`, `RCC_OscInitTypeDef`) for peripheral config
- Use `enum` for state machines
- Avoid `void*` unless absolutely necessary for callbacks
