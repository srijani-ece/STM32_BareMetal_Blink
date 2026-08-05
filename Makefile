# ELI5: "make" reads this file so you can just type `make` instead of
# typing out this whole compiler command by hand every time.

TARGET = blink
CC     = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

# -mcpu=cortex-m0plus : tell the compiler exactly which CPU we're using
# -mthumb             : use the compact Thumb instruction set (all Cortex-M chips use this)
# -nostdlib -nostartfiles : we wrote our own startup code, don't link the normal C runtime
# -T linker.ld         : use OUR memory map, not a default one
CFLAGS = -mcpu=cortex-m0plus -mthumb -Wall -Wextra -O2 -ffreestanding -nostdlib -nostartfiles
LDFLAGS = -T linker.ld -nostdlib -nostartfiles

SRCS = startup.c main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET).elf $(TARGET).bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f *.o $(TARGET).elf $(TARGET).bin

.PHONY: all clean
