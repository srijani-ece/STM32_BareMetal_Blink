// main.c — blinks the onboard LED (PA5) on an ST Nucleo-C031C6 by
// writing directly to memory-mapped hardware registers. No HAL, no
// CMSIS GPIO functions, no vendor libraries.

// ─── Register Definitions (STM32C031C6 specific) ───────────────────────
#define RCC_BASE      0x40021000UL
#define GPIOA_BASE    0x50000000UL

#define RCC_IOPENR    (*(volatile unsigned int *)(RCC_BASE   + 0x34))
#define GPIOA_MODER   (*(volatile unsigned int *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR     (*(volatile unsigned int *)(GPIOA_BASE + 0x14))

// ─── Software Delay ─────────────────────────────────────────────────────
// volatile loop counter prevents the compiler from optimizing this away
void delay(volatile int count) {
    while (count--) {
        __asm__("nop"); // ARM No-Operation — one cycle, never optimized out
    }
}

int main(void) {
    // Step 1: Enable peripheral clock for GPIOA.
    // RCC_IOPENR bit 0 = GPIOAEN. Peripherals are clock-gated by default —
    // without this, all GPIOA register writes are silently ignored.
    RCC_IOPENR |= (1 << 0);

    // Step 2: Configure PA5 as General Purpose Output.
    // MODER register: 2 bits per pin. Bits [11:10] control PA5.
    // 00 = Input, 01 = Output, 10 = Alternate Function, 11 = Analog
    GPIOA_MODER &= ~(3U << 10);  // clear bits 11:10
    GPIOA_MODER |=  (1U << 10);  // set bits 11:10 = 01 (Output)

    // Step 3: Blink loop.
    while (1) {
        GPIOA_ODR ^= (1U << 5);  // XOR toggle PA5
        delay(500000);            // ~500ms at 48MHz with NOP delay
    }
}
