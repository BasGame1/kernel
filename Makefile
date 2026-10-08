CC = riscv64-unknown-elf-gcc
CFLAGS ?= -fcommon -Wall -Wextra -ffreestanding -nostdlib -mcmodel=medany -march=rv32imac_zicsr -mabi=ilp32
OUT = $(CURDIR)/out
OUT_TESTS = $(OUT)/tests
TARGET = $(OUT)/kernel
TARGET_TESTS = $(OUT_TESTS)/run_tests
DIRS = assembly clock screen memory utils
OBJECTS = $(foreach dir,$(DIRS),$(OUT)/$(dir).o)
KERNEL_OBJECT = $(OUT)/kernel.o
ALL_OBJECTS = $(OBJECTS) $(KERNEL_OBJECT)
TEST_FILES = $(wildcard tests/*.c)
TEST_OBJECTS = $(patsubst tests/%.c,$(OUT_TESTS)/%.o,$(TEST_FILES))
LINKER = kernel.ld
GEN_LINKER = $(OUT)/kernel.ld

all: $(OUT) dirs $(KERNEL_OBJECT) $(TARGET)

qemu: CFLAGS += -D__QEMU__
qemu: all

arch64: CFLAGS -= -march=rv32imac_zicsr -mabi=ilp32
arch64: all

debug: CFLAGS += -D__DEBUG__
debug: TARGET = $(OUT)/kernel-debug
debug: all

tests: $(OUT) $(OUT_TESTS) dirs_with_tests $(TARGET_TESTS)

$(OUT):
	mkdir -p $(OUT)

$(OUT_TESTS):
	mkdir -p $(OUT_TESTS)

dirs: $(OUT)
	@for dir in $(DIRS); do $(MAKE) -C $$dir CC="$(CC)" CFLAGS="$(CFLAGS)" OUT=$(OUT) || exit 1; done

dirs_with_tests: $(OUT)
	@for dir in $(DIRS); do $(MAKE) -C $$dir CC="$(CC)" CFLAGS="$(CFLAGS)" OUT=$(OUT) || exit 1; done

$(KERNEL_OBJECT): kernel.c | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_TESTS)/%.o: tests/%.c | $(OUT_TESTS)
	$(CC) $(CFLAGS) -c $< -o $@

$(GEN_LINKER): $(LINKER) | $(OUT)
	sed -e 's/OUTPUT_ARCH[[:space:]]*=[[:space:]]*/OUTPUT_ARCH/g' \
	    -e 's/ENTRY[[:space:]]*=[[:space:]]*/ENTRY/g' \
	    -e 's/SECTIONS[[:space:]]*=[[:space:]]*/SECTIONS/g' \
	    -e 's/\(\.[a-zA-Z0-9_.]\+\)[[:space:]]*=[[:space:]]*{/\1 : {/g' $< > $@

$(TARGET): $(ALL_OBJECTS) $(GEN_LINKER)
	$(CC) $(CFLAGS) -nostdlib -T $(GEN_LINKER) -o $@ $(ALL_OBJECTS)

$(TARGET_TESTS): $(TEST_OBJECTS) $(OBJECTS) $(GEN_LINKER)
	$(CC) $(CFLAGS) -nostdlib -T $(GEN_LINKER) -o $@ $^

clean:
	@for dir in $(DIRS); do $(MAKE) -C $$dir clean OUT=$(OUT); done
	rm -rf $(OUT)

.PHONY: all dirs dirs_with_tests tests clean

