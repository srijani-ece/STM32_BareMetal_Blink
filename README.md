# STM32 Bare-Metal Blink: No HAL, No CMSIS, From Boot to Blink

**Problem:** Every embedded tutorial starts with `HAL_GPIO_TogglePin()`. That
single function call hides four layers of abstraction — clock trees,
register offsets, bit masking, and peripheral enable logic — AND it hides
an even bigger thing: how the chip got from "power applied" to "your
code is running" at all. This project doesn't skip that part.
**Solution:** A complete, from-scratch boot chain for an
**ST Nucleo-C031C6** (STM32C031C6, ARM Cortex-M0+ @ 48 MHz):
a real vector table, a real `Reset_Handler`, a real linker script and
then, at the very end of all that, a blinking LED on PA5.

## What actually happens when you power on the chip:

Imagine the microcontroller as a brand new employee showing up for their
first day with total amnesia, no idea what building they're in, what
their desk looks like, or what their job is. Here's the onboarding, in
order:

1. **The chip wakes up and looks at address 0.** Hardwired into the
   silicon: "check address 0 for my instructions." That's the
   **vector table** (`startup.c`) — literally the first thing in Flash
   memory (enforced by `linker.ld`). It contains two things: where the
   stack (scratch space) starts, and where to jump to first.

2. **`Reset_Handler` runs.** This is the "new employee onboarding."
   Before your actual program can safely touch any variable, two things
   have to happen:
   - Any global variable that starts with a real value
     (`int x = 5;`) needs that `5` copied from Flash into RAM, because
     RAM is empty/garbage on power-up.
   - Any global variable with no starting value (`int counter;`) needs
     to be zeroed out, because C guarantees these start at 0, and RAM
     is full of leftover garbage bits from nothing in particular.

3. **`main()` finally runs.** Only now is it safe to run real code —
   which in this project means: turn on the GPIOA clock, configure
   PA5 as an output, and blink it forever.

Every single-file "blink" tutorial online skips steps 1 and 2 entirely,
because a vendor HAL or CMSIS startup file does it silently for you.
This project makes all of it visible and hand-written.

## Files

| File | What it does |
|---|---|
| `linker.ld` | Memory map: tells the toolchain where Flash and RAM live, and exactly where the vector table must sit |
| `startup.c` | Vector table + `Reset_Handler` — the code that runs before `main()` |
| `main.c` | The actual blink logic — direct register writes, no HAL |
| `Makefile` | Builds everything into a flashable `.bin` / `.elf` |

## Registers used

| Register | Address | Purpose |
|---|---|---|
| `RCC_IOPENR` | `0x40021000 + 0x34` | Enable clock for GPIOA (bit 0) |
| `GPIOA_MODER` | `0x50000000 + 0x00` | Set PA5 as output (bits 11:10 = 01) |
| `GPIOA_ODR` | `0x50000000 + 0x14` | Toggle PA5 output state (bit 5) |

> STM32C031C6 uses `RCC_IOPENR` (not `RCC_AHB1ENR`, which is an F4-series
> register), and GPIOA's base address is `0x50000000` on C0-series chips
> — different from the F1/F4 families most tutorials assume.

## How to build it

You need the ARM GNU toolchain (`arm-none-eabi-gcc`), which isn't
installed in every environment by default:

```bash
# Debian/Ubuntu
sudo apt install gcc-arm-none-eabi

# then, from this project folder:
make
```

This produces `blink.elf` (for debugging/simulation) and `blink.bin`
(the raw binary you'd flash to real hardware).

>> **Verified:** builds cleanly with `arm-none-eabi-gcc`, zero warnings.
> Confirmed via `objdump -h blink.elf`:
> - `.isr_vector` sits at `0x08000000` (start of Flash), 0x40 bytes = 16
>   entries — exactly matches the vector table defined in `startup.c`
> - `.text` begins immediately at `0x08000040` with zero padding
> - `blink.bin` is 260 bytes — exactly `.isr_vector` (64B) + `.text`
>   (196B), byte-for-byte accounted for
> - Disassembly of `Reset_Handler` confirms the `.data` copy loop
>   compiles to the expected `ldr`/`str`/`cmp`/`bne` sequence (this
>   project has no initialized globals, so the loop correctly does
>   nothing at runtime — proof the logic is conditionally correct, not
>   just present)

## Build output:
BUILD OUTPUT: `blink.elf` (debug/simulation) and `blink.bin` (260 bytes, flashable) — see verification details above.
<img width="819" height="460" alt="objdump-verification" src="https://github.com/user-attachments/assets/bd07ef7a-6b72-4df5-90af-11b42fac34be" />

## Why `ODR ^=` instead of `BSRR`?

`GPIOA_ODR ^= (1 << 5)` works fine for a simple polling loop like this
one. But in production code — especially inside an interrupt handler or
alongside an RTOS — you should use `GPIOA_BSRR` (Bit Set/Reset Register)
instead. `ODR` requires a read-modify-write cycle, which can race with
an interrupt that also touches `ODR` mid-operation. `BSRR` sets or clears
individual bits atomically at the hardware level, with no race window.

## Why register pointers need `volatile`

```c
// WITHOUT volatile:
unsigned int *reg = (unsigned int *)0x50001000;
*reg = 1;
*reg = 2;    // compiler may delete the first write — "why write twice
             //  to a variable nothing else reads in between?"

// WITH volatile:
volatile unsigned int *reg = (volatile unsigned int *)0x50001000;
*reg = 1;
*reg = 2;    // compiler MUST emit both writes
```

Hardware registers can change independent of your code (interrupts,
DMA, the physical world). `volatile` tells the compiler "don't assume
you know better — actually perform this read/write every time."

## What I'd add next

- Wire this up in the Wokwi simulator so it's runnable without physical
  hardware
- Replace the NOP busy-wait delay with a SysTick-timer-based delay (DONE/UPDATED)
- Add GPIOA interrupt handling (EXTI) as a second example, to exercise
  the vector table entries beyond Reset/SysTick

## Technical Notes & Lessons Learned
**Hardware Timer vs. NOP Loop:** I initially estimated the LED blink timing using a `NOP` loop based on the chip's maximum 48 MHz clock speed. However, I caught that the STM32C0 actually boots at `HSISYS / 4` (12 MHz) by default out of reset. Instead of hand-tuning and guessing the `NOP` instruction cycles, I refactored the delay to use the deterministic Cortex-M0+ `SysTick` hardware timer.

**Power Efficiency:** The `delay_ms()` function utilizes the `WFI` (Wait For Interrupt) assembly instruction, putting the CPU core to sleep between ticks rather than burning battery cycles in a busy-wait loop.
