// main.c — blinks the onboard LED (PA5) on an ST Nucleo-C031C6 by
// writing directly to memory-mapped hardware registers. No HAL, no
// CMSIS GPIO functions, no vendor libraries.

#include <stdint.h>

// ─── Register Definitions (STM32C031C6 specific) ───────────────────────
#define RCC_BASE      0x40021000UL
#define GPIOA_BASE    0x50000000UL

#define RCC_IOPENR    (*(volatile unsigned int *)(RCC_BASE   + 0x34))
#define GPIOA_MODER   (*(volatile unsigned int *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR     (*(volatile unsigned int *)(GPIOA_BASE + 0x14))

// ─── SysTick Registers ──────────────────────────────────────────────────
// SysTick isn't chip-specific — it's part of the ARM Cortex-M core itself,
// so this address is the same on every Cortex-M0/M0+/M3/M4/M7 chip ever
// made, regardless of vendor (ST, NXP, TI, whoever).
#define SYSTICK_BASE  0xE000E010UL
#define SYSTICK_CTRL  (*(volatile unsigned int *)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD  (*(volatile unsigned int *)(SYSTICK_BASE + 0x04))
#define SYSTICK_VAL   (*(volatile unsigned int *)(SYSTICK_BASE + 0x08))

#define SYSTICK_CTRL_ENABLE    (1U << 0) // start the counter
#define SYSTICK_CTRL_TICKINT   (1U << 1) // fire an interrupt when it hits 0
#define SYSTICK_CTRL_CLKSOURCE (1U << 2) // 1 = use the CPU clock directly

// STM32C0 boots on HSISYS = HSI48 divided by 4 = 12 MHz. This project
// never touches the RCC to change that, so this is the real, correct
// clock speed the CPU is running at right now.
#define SYSTEM_CLOCK_HZ 12000000UL

// ─── The Millisecond Clock ──────────────────────────────────────────────
// ELI5: think of SysTick as a tiny, dumb kitchen timer built into the
// CPU. We wind it up to "ding" exactly once every 1ms. Every time it
// dings, the CPU automatically jumps to SysTick_Handler() below, which
// just adds 1 to a counter. That counter is now an accurate clock we
// can check any time we want — instead of a NOP loop, which has no
// real relationship to actual elapsed time at all (it just burns
// roughly-guessed CPU cycles).
//
// 'volatile' here matters for a DIFFERENT reason than the NOP delay did:
// msTicks is modified inside an interrupt handler (SysTick_Handler),
// completely outside the normal flow of delay_ms(). Without 'volatile',
// the compiler might cache msTicks in a register inside delay_ms()'s
// while-loop and never notice the interrupt changed the real value in
// memory — causing an infinite loop.
volatile uint32_t msTicks = 0;

// This function's name and signature must exactly match the weak alias
// declared in startup.c. Because this is now a real, non-weak definition
// of SysTick_Handler, the linker uses THIS version instead of the
// Default_Handler fallback — no other change needed in startup.c.
void SysTick_Handler(void) {
    msTicks++;
}

static void SysTick_Init(void) {
    // Reload value = how many CPU cycles = 1ms. At 12,000,000 Hz,
    // 12,000,000 / 1000 = 12,000 cycles per millisecond. Subtract 1
    // because the counter counts DOWN to zero inclusive (0..11999 is
    // 12,000 counts).
    SYSTICK_LOAD = (SYSTEM_CLOCK_HZ / 1000) - 1;
    SYSTICK_VAL  = 0; // clear current value before starting
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;
}

// Accurate blocking delay, in milliseconds — replaces the old NOP loop.
static void delay_ms(uint32_t ms) {
    uint32_t start = msTicks;
    while ((msTicks - start) < ms) {
        // Spin here, but now we're waiting on REAL elapsed time (driven
        // by the hardware timer + interrupt), not a guessed cycle count.
        // Unsigned subtraction here also safely handles msTicks wrapping
        // around back to 0 after ~49 days of uptime.
    }
}

int main(void) {
    // Step 1: Enable peripheral clock for GPIOA.
    RCC_IOPENR |= (1 << 0);

    // Step 2: Configure PA5 as General Purpose Output.
    GPIOA_MODER &= ~(3U << 10);
    GPIOA_MODER |=  (1U << 10);

    // Step 3: Start the millisecond clock.
    SysTick_Init();

    // Step 4: Blink loop, now timed by real hardware milliseconds.
    while (1) {
        GPIOA_ODR ^= (1U << 5);
        delay_ms(500);
    }
}