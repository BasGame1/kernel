CC ?= gcc
CFLAGS ?=

ifeq ($(findstring -D__ARM__,$(CFLAGS)),-D__ARM__)
BOOT_SRC = arm/arch_arm_entry.S
OBJECTS = boot.o umode.o
else ifeq ($(findstring -D__X86__,$(CFLAGS)),-D__X86__)
BOOT_SRC = x86/arch_x86_entry.S
OBJECTS = boot.o umode.o
else ifeq ($(findstring -D__64BIT__,$(CFLAGS)),-D__64BIT__)
BOOT_SRC = riscv/arch_riscv64_entry.S
OBJECTS = boot.o umode.o
else
BOOT_SRC = riscv/arch_riscv32_entry.S
OBJECTS = boot.o trap.o umode.o
endif
COMBINED_OBJECTS = $(OUT)/$(notdir $(CURDIR)).o

all: $(COMBINED_OBJECTS)

boot.o: $(BOOT_SRC)
	sed -e 's/\.[[:space:]]\+/./g' $< | $(CC) $(CFLAGS) -c -x assembler-with-cpp - -o $@

trap.o: trap.S
	$(CC) $(CFLAGS) -c $< -o $@

umode.o: umode.S
	$(CC) $(CFLAGS) -c $< -o $@

$(COMBINED_OBJECTS): $(OBJECTS)
	$(CC) $(CFLAGS) -nostdlib -r -o $@ $^
	rm -f $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(COMBINED_OBJECTS)

