**Problem:**
Every embedded tutorial starts with HAL_GPIO_TogglePin(). That single function
call hides four layers of abstraction like clock trees, register offsets, bit masking,
and peripheral enable logic. When something breaks in production, you need to know
exactly which bit in which register to flip. This project strips everything away and
talks directly to the hardware.
<BR>
<br>
**Solution:**
Blink the onboard LED (PA5) on an **ST Nucleo-C031C6** by writing directly to the
STM32C031C6's memory-mapped I/O registers which means no HAL, no CMSIS GPIO functions,
no vendor libraries. Three registers. Thirty lines of C.
<br>
<br>
**Hardware:**
Board - <i>ST Nucleo-C031C6</i>
<br>
MCU - <i>STM32C031C6 (ARM Cortex-M0+ @ 48 MHz)</i>
<br>
LED - <i>LD4 — onboard, connected to **PA5**</i>
<br>
Simulation - <i>Wokwi(https://www.wokwi.com) template</i>
<br>
<br>
**Registers Used**

| Register | Address | Purpose |
| :--- | :--- | :--- |
| `RCC_IOPENR` | `0x40021000 + 0x34` | Enable clock for GPIOA (bit 0) |
| `GPIOA_MODER` | `0x50000000 + 0x00` | Set PA5 as General Purpose Output (bits 11:10 = 01) |
| `GPIOA_ODR` | `0x50000000 + 0x14` | Toggle PA5 output state (bit 5) |

> **Note:** STM32C031C6 uses `RCC_IOPENR` (not `RCC_AHB1ENR` which is an F4-series register). 
> GPIOA base address is `0x50000000` on C0-series — different from F1/F4 families.
>

<br>

**Implementation**

```c
// ─── Register Definitions (STM32C031C6 specific) ────────────────────────────
#define RCC_BASE      0x40021000UL
#define GPIOA_BASE    0x50000000UL

#define RCC_IOPENR    (*(volatile unsigned int *)(RCC_BASE  + 0x34))
#define GPIOA_MODER   (*(volatile unsigned int *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR     (*(volatile unsigned int *)(GPIOA_BASE + 0x14))

// ─── Software Delay ─────────────────────────────────────────────────────────
// volatile loop counter prevents compiler from optimising this away at -O2
void delay(volatile int count) {
    while (count--) {
        __asm__("nop");   // ARM No-Operation — one cycle, never optimised out
    }
}

// ─── Main ───────────────────────────────────────────────────────────────────
int main(void) {

    // Step 1: Enable peripheral clock for GPIOA
    // RCC_IOPENR bit 0 = GPIOAEN. Peripherals are clock-gated by default.
    // Without this, all GPIOA register writes are silently ignored.
    RCC_IOPENR |= (1 << 0);

    // Step 2: Configure PA5 as General Purpose Output
    // MODER register: 2 bits per pin. Bits [11:10] control PA5.
    // 00 = Input, 01 = Output, 10 = Alternate Function, 11 = Analog
    GPIOA_MODER &= ~(3U << 10);   // Clear bits 11:10 (avoid leaving invalid state)
    GPIOA_MODER |=  (1U << 10);   // Set bits 11:10 = 01 (Output)

    // Step 3: Blink loop
    while (1) {
        GPIOA_ODR ^= (1U << 5);   // XOR toggle PA5
        delay(500000);             // ~500ms at 48MHz with NOP delay
    }
}
```
<br>

**Architecture**
<br>

```text
Power-On Reset
      |
      ▼
RCC_IOPENR |= (1 << 0)          ← Enable GPIOA clock gate
      |
      ▼
GPIOA_MODER bits [11:10] = 01   ← PA5 = Output mode
      |
      ▼
 ┌──────────────────────────┐
 │  Infinite Loop           │
 │  GPIOA_ODR ^= bit5       │  ← Toggle PA5 (XOR — atomic bit flip)
 │  delay(500000)           │  ← NOP software delay
 └──────────────┬───────────┘
                └─ repeat
```
**Why use GPIOA_ODR XOR instead of separate SET/CLEAR?** <br>

Using `ODR ^= Pin` works perfectly fine if you just need a quick-and-dirty toggle. However, in production code—especially inside an Interrupt Service Routine (ISR) or a multi-threaded RTOS—you should use `GPIOA_BSRR` (Bit Set/Reset Register) instead.

`BSRR` allows for **atomic** bit manipulation. Because `ODR` modification requires a read-modify-write cycle, it is prone to race conditions if an interrupt hits right in the middle of the operation. `BSRR` avoids this entirely at the hardware level.

**Why register pointers require `volatile`** <br>

```c
// WITHOUT volatile:
unsigned int *reg = (unsigned int *)0x50001000;
*reg = 1;
*reg = 2;    // Compiler may eliminate the first write

// WITH volatile:
volatile unsigned int *reg = (volatile unsigned int *)0x50001000;
*reg = 1;
*reg = 2;    // Compiler MUST emit both writes
```
The `volatile` keyword prevents the compiler from optimizing your code into oblivion. Without it, the compiler assumes a variable only changes when the software explicitly modifies it, leading it to cache the value in a CPU register for efficiency.

Because memory-mapped hardware registers can change independently of your code (via hardware events, ISRs, or DMA), `volatile` forces the CPU to perform an actual read or write operation every single time the register is accessed.

**Clock Gating: Enable the clock first:**
To save power, STM32 chips keep all peripherals completely powered down (clock-gated) by default. If you try to write to a GPIO register before enabling its clock in `RCC_IOPENR`, the hardware simply ignores the write because it has no clock signal to process the instruction. Forgetting to turn on the peripheral clock is easily the most common pitfall when starting out with bare-metal programming.
