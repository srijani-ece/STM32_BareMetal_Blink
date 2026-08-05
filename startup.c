/*
 * startup.c — the very first code that runs when the chip powers on.
 *
 * ELI5: Before your main() can run, someone has to:
 *   1. Tell the CPU "here's where the stack lives, and here's the first
 *      function to jump to" — that's the VECTOR TABLE.
 *   2. Copy your initialized global variables from Flash into RAM
 *      (Flash keeps its data after power-off, RAM doesn't — so any
 *      variable that starts with a real value like `int x = 5;` has to
 *      be re-copied into RAM every time the chip boots).
 *   3. Zero out any global variables that don't have a starting value
 *      (like `int counter;`) — RAM is full of leftover garbage bits
 *      from whatever ran before, so C requires these to start at 0).
 *   4. THEN, finally, call main().
 *
 * This file does steps 1-4. It's the "hidden" code that every HAL/CMSIS
 * project normally does for you without showing you.
 */

#include <stdint.h>

extern uint32_t _estack;     // top of RAM, defined in linker.ld
extern uint32_t _sidata;     // where .data's initial values live in Flash
extern uint32_t _sdata;      // start of .data in RAM
extern uint32_t _edata;      // end of .data in RAM
extern uint32_t _sbss;       // start of .bss in RAM
extern uint32_t _ebss;       // end of .bss in RAM

int main(void); // defined in main.c

void Reset_Handler(void);
void Default_Handler(void);

/* --- Step 2 & 3 & 4 --- */
void Reset_Handler(void) {
    // Copy .data section from Flash into RAM, word by word.
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    // Zero out .bss section in RAM.
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    // Now it's safe to run C code that touches global variables.
    main();

    // main() should never return on an embedded target, but just in
    // case, sit here forever instead of executing garbage memory.
    while (1) { }
}

/* Fallback handler for any interrupt we haven't specifically written
   a handler for. If an unexpected interrupt fires, we land here and
   spin, instead of the CPU jumping into random memory. */
void Default_Handler(void) {
    while (1) { }
}

/* Weak aliases: every interrupt in the table below points to
   Default_Handler UNLESS you define a real function with that name
   elsewhere in your code (e.g. write "void SysTick_Handler(void) {...}"
   in main.c and it will automatically override this). */
#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))

void NMI_Handler(void) WEAK_ALIAS;
void HardFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void) WEAK_ALIAS;
void PendSV_Handler(void) WEAK_ALIAS;
void SysTick_Handler(void) WEAK_ALIAS;

/* --- Step 1: the vector table itself --- */
/* This MUST be placed at the very start of Flash (see linker.ld,
   section .isr_vector). On reset, the Cortex-M0+ hardware reads:
     address 0x00 -> initial value to load into the Stack Pointer
     address 0x04 -> address of the first function to jump to
   That's why _estack and Reset_Handler are entries [0] and [1]. */
__attribute__((section(".isr_vector")))
void (* const vector_table[])(void) = {
    (void (*)(void))&_estack,   // [0] initial stack pointer
    Reset_Handler,               // [1] Reset
    NMI_Handler,                  // [2] NMI
    HardFault_Handler,            // [3] Hard Fault
    0, 0, 0, 0, 0, 0, 0,          // [4-10] reserved on Cortex-M0+
    SVC_Handler,                   // [11] SVCall
    0, 0,                           // [12-13] reserved
    PendSV_Handler,                 // [14] PendSV
    SysTick_Handler,                 // [15] SysTick
    // [16+] would be chip-specific peripheral interrupts (GPIO, timers,
    // etc.) — omitted here since this project doesn't use interrupts yet.
};
