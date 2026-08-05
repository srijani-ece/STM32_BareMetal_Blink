/*
 * Linker script for STM32C031C6 (ARM Cortex-M0+, 32KB Flash, 12KB RAM)
 *
 * ELI5: A linker script is a MAP that tells the toolchain where things
 * live in the chip's memory. Think of the chip as an apartment building
 * with two floors:
 *   - FLASH  = the "permanent storage" floor. Your compiled code lives
 *              here. It survives power-off. Read-only while running.
 *   - RAM    = the "scratch pad" floor. Variables that change while the
 *              program runs live here. Erased on power-off.
 *
 * Without this file, the linker has no idea these two floors even exist,
 * or which one to put things on.
 */

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 32K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 12K
}

/* Where the initial stack pointer starts: the very top of RAM.
   The stack grows DOWNWARD from here as functions are called. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  /* .isr_vector: the vector table MUST be the very first thing in
     Flash (address 0x08000000) — the CPU hardware reads address 0
     (aliased to Flash start) on reset to find the initial stack
     pointer and the address of Reset_Handler. If this isn't first,
     the chip won't boot at all. */
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  /* .text: your actual compiled code (main, functions, etc.) */
  .text :
  {
    *(.text)
    *(.text*)
  } > FLASH

  /* .rodata: constants (e.g. "const int x = 5;", string literals) */
  .rodata :
  {
    *(.rodata)
    *(.rodata*)
  } > FLASH

  /* _sidata marks WHERE in Flash the initial values for .data live.
     _sdata / _edata mark the START and END of .data's home in RAM.
     Reset_Handler copies bytes from _sidata into [_sdata, _edata). */
  _sidata = LOADADDR(.data);

  .data :
  {
    _sdata = .;
    *(.data)
    *(.data*)
    _edata = .;
  } > RAM AT > FLASH

  /* .bss: zero-initialized globals (e.g. "int counter;" with no value).
     These don't need to be copied from Flash — just zeroed at boot.
     _sbss / _ebss mark this region so Reset_Handler knows what to clear. */
  .bss :
  {
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM
}
