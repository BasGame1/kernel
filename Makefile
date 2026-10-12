# 1. Determine requested architecture (REQ_ARCH) from targets or command-line variables
ifneq ($(filter x86,$(MAKECMDGOALS)),)
  REQ_ARCH := x86
else ifneq ($(filter x86_64,$(MAKECMDGOALS)),)
  REQ_ARCH := x86_64
else ifneq ($(filter arm,$(MAKECMDGOALS)),)
  REQ_ARCH := arm
else ifneq ($(filter arm64,$(MAKECMDGOALS)),)
  REQ_ARCH := arm64
else ifneq ($(filter riscv,$(MAKECMDGOALS)),)
  REQ_ARCH := riscv
else ifneq ($(filter riscv64,$(MAKECMDGOALS)),)
  REQ_ARCH := riscv64
else ifneq ($(strip $(ARCH)),)
  REQ_ARCH := $(strip $(ARCH))
endif

# 2. Overwrite or validate CONFIG against requested architecture
CONFIG_EXPLICIT := $(findstring command line,$(origin CONFIG))

ifeq ($(CONFIG_EXPLICIT),)
  # CONFIG was not explicitly passed on the command line; overwrite it based on REQ_ARCH
  ifeq ($(REQ_ARCH),x86)
    CONFIG := default-x86
  else ifeq ($(REQ_ARCH),x86_64)
    CONFIG := default-x86_64
  else ifeq ($(REQ_ARCH),arm)
    CONFIG := default-arm
  else ifeq ($(REQ_ARCH),arm64)
    CONFIG := default-arm64
  else ifeq ($(REQ_ARCH),riscv)
    CONFIG := default-riscv
  else ifeq ($(REQ_ARCH),riscv64)
    CONFIG := default-riscv64
  else
    CONFIG ?= default-riscv
  endif
endif

CONFIG_FILE = $(CURDIR)/config/$(CONFIG)
ifeq ($(wildcard $(CONFIG_FILE)),)
  $(error Config file '$(CONFIG_FILE)' does not exist!)
endif

# 3. Read architecture flags from CONFIG_FILE
CFG_RISCV := $(shell grep '^[[:space:]]*RISCV=' $(CONFIG_FILE) 2>/dev/null | head -n1 | cut -d= -f2 | cut -d' ' -f1 | tr -d '\r')
CFG_ARM   := $(shell grep '^[[:space:]]*ARM=' $(CONFIG_FILE) 2>/dev/null | head -n1 | cut -d= -f2 | cut -d' ' -f1 | tr -d '\r')
CFG_X86   := $(shell grep '^[[:space:]]*X86=' $(CONFIG_FILE) 2>/dev/null | head -n1 | cut -d= -f2 | cut -d' ' -f1 | tr -d '\r')
CFG_32BIT := $(shell grep '^[[:space:]]*32BIT=' $(CONFIG_FILE) 2>/dev/null | head -n1 | cut -d= -f2 | cut -d' ' -f1 | tr -d '\r')

# 4. If an architecture was requested and CONFIG was explicitly specified, verify compatibility or error out!
ifneq ($(REQ_ARCH),)
  ifneq ($(CONFIG_EXPLICIT),)
    ifeq ($(REQ_ARCH),x86)
      ifneq ($(strip $(CFG_X86)),1)
        $(error Architecture mismatch: CONFIG='$(CONFIG)' is configured for a different architecture (RISCV=$(CFG_RISCV), ARM=$(CFG_ARM), X86=$(CFG_X86)), but x86 was requested!)
      endif
    else ifeq ($(REQ_ARCH),x86_64)
      ifneq ($(strip $(CFG_X86)),1)
        $(error Architecture mismatch: CONFIG='$(CONFIG)' is configured for a different architecture (RISCV=$(CFG_RISCV), ARM=$(CFG_ARM), X86=$(CFG_X86)), but x86_64 was requested!)
      endif
    else ifeq ($(REQ_ARCH),riscv)
      ifneq ($(strip $(CFG_RISCV)),1)
        $(error Architecture mismatch: CONFIG='$(CONFIG)' is configured for a different architecture (RISCV=$(CFG_RISCV), ARM=$(CFG_ARM), X86=$(CFG_X86)), but riscv was requested!)
      endif
    else ifeq ($(REQ_ARCH),riscv64)
      ifneq ($(strip $(CFG_RISCV)),1)
        $(error Architecture mismatch: CONFIG='$(CONFIG)' is configured for a different architecture (RISCV=$(CFG_RISCV), ARM=$(CFG_ARM), X86=$(CFG_X86)), but riscv64 was requested!)
      endif
    else ifeq ($(REQ_ARCH),arm)
      ifneq ($(strip $(CFG_ARM)),1)
        $(error Architecture mismatch: CONFIG='$(CONFIG)' is configured for a different architecture (RISCV=$(CFG_RISCV), ARM=$(CFG_ARM), X86=$(CFG_X86)), but arm was requested!)
      endif
    else ifeq ($(REQ_ARCH),arm64)
      ifneq ($(strip $(CFG_ARM)),1)
        $(error Architecture mismatch: CONFIG='$(CONFIG)' is configured for a different architecture (RISCV=$(CFG_RISCV), ARM=$(CFG_ARM), X86=$(CFG_X86)), but arm64 was requested!)
      endif
    endif
  endif
else
  # If no REQ_ARCH was explicitly given, derive TARGET_ARCH from config file
  ifeq ($(strip $(CFG_X86)),1)
    ifeq ($(strip $(CFG_32BIT)),1)
      REQ_ARCH := x86
    else
      REQ_ARCH := x86_64
    endif
  else ifeq ($(strip $(CFG_ARM)),1)
    ifeq ($(strip $(CFG_32BIT)),1)
      REQ_ARCH := arm
    else
      REQ_ARCH := arm64
    endif
  else
    ifeq ($(strip $(CFG_32BIT)),0)
      REQ_ARCH := riscv64
    else
      REQ_ARCH := riscv
    endif
  endif
endif

CONFIG_CC := $(shell grep '^[[:space:]]*CC=' $(CONFIG_FILE) 2>/dev/null | head -n1 | cut -d= -f2 | cut -d' ' -f1 | tr -d '\r')

CONFIG_USE_ANTARTICA := $(shell grep '^[[:space:]]*USE_ANTARTICA=' $(CONFIG_FILE) 2>/dev/null | head -n1 | cut -d= -f2 | cut -d' ' -f1 | tr -d '\r')
ifeq ($(strip $(CONFIG_USE_ANTARTICA)),1)
ANTARTICA_DIR = antartica
CFLAGS_ANTARTICA = -D__USE_ANTARTICA__
else
ANTARTICA_DIR =
CFLAGS_ANTARTICA =
endif

# Toolchain and flags mapping by architecture
ifeq ($(REQ_ARCH),x86)
  DEF_CC = gcc -m32
  DEF_CFLAGS = -fcommon -Wall -Wextra -ffreestanding -nostdlib -m32 -fno-stack-protector -D__X86__ -D__32BIT__ $(CFLAGS_ANTARTICA)
  DEF_RUST_TARGET = i686-unknown-linux-gnu
  LD_ARCH = i386
  LD_ENTRY = _start_x86
  LD_BASE = 0x00100000
else ifeq ($(REQ_ARCH),x86_64)
  DEF_CC = gcc
  DEF_CFLAGS = -fcommon -Wall -Wextra -ffreestanding -nostdlib -m64 -fno-stack-protector -D__X86__ -D__64BIT__ $(CFLAGS_ANTARTICA)
  DEF_RUST_TARGET = x86_64-unknown-none
  LD_ARCH = i386:x86-64
  LD_ENTRY = _start_x86
  LD_BASE = 0x00100000
else ifeq ($(REQ_ARCH),arm)
  DEF_CC = arm-none-eabi-gcc
  DEF_CFLAGS = -fcommon -Wall -Wextra -ffreestanding -nostdlib -fno-stack-protector -D__ARM__ -D__32BIT__ $(CFLAGS_ANTARTICA)
  DEF_RUST_TARGET = armv7a-none-eabi
  LD_ARCH = arm
  LD_ENTRY = _start_arm
  LD_BASE = 0x40000000
else ifeq ($(REQ_ARCH),arm64)
  DEF_CC = aarch64-linux-gnu-gcc
  DEF_CFLAGS = -fcommon -Wall -Wextra -ffreestanding -nostdlib -fno-stack-protector -D__ARM__ -D__64BIT__ $(CFLAGS_ANTARTICA)
  DEF_RUST_TARGET = aarch64-unknown-none-softfloat
  LD_ARCH = aarch64
  LD_ENTRY = _start_arm
  LD_BASE = 0x40000000
else ifeq ($(REQ_ARCH),riscv64)
  DEF_CC = riscv64-unknown-elf-gcc
  DEF_CFLAGS = -fcommon -Wall -Wextra -ffreestanding -nostdlib -fno-stack-protector -mcmodel=medany -march=rv64gc -mabi=lp64d -D__RISCV__ -D__64BIT__ -D__QEMU__ $(CFLAGS_ANTARTICA)
  DEF_RUST_TARGET = riscv64gc-unknown-none-elf
  LD_ARCH = riscv
  LD_ENTRY = _start64
  LD_BASE = 0x80000000
else
  # Default: riscv 32
  DEF_CC = riscv64-unknown-elf-gcc
  DEF_CFLAGS = -fcommon -Wall -Wextra -ffreestanding -nostdlib -fno-stack-protector -mcmodel=medany -march=rv32imac_zicsr_zifencei -mabi=ilp32 -D__RISCV__ -D__32BIT__ -D__QEMU__ $(CFLAGS_ANTARTICA)
  DEF_RUST_TARGET = riscv32imc-unknown-none-elf
  LD_ARCH = riscv
  LD_ENTRY = _start
  LD_BASE = 0x80000000
endif

ifneq ($(strip $(CONFIG_CC)),)
  CC = $(strip $(CONFIG_CC))
else ifeq ($(origin CC),default)
  CC = $(DEF_CC)
else
  CC ?= $(DEF_CC)
endif
CFLAGS ?= $(DEF_CFLAGS)
RUST_TARGET ?= $(DEF_RUST_TARGET)

OUT = $(CURDIR)/out
OUT_TESTS = $(OUT)/tests
TARGET = $(OUT)/kernel
TARGET_TESTS = $(OUT_TESTS)/run_tests
DIRS = assembly clock screen memory utils keyboard shell filesystem userspace updates drivers $(ANTARTICA_DIR)
DIRS_ALL = assembly clock screen memory utils keyboard shell filesystem userspace updates drivers antartica
OBJECTS = $(foreach dir,$(DIRS),$(OUT)/$(dir).o)
KERNEL_OBJECT = $(OUT)/kernel.o
TEST_FILES = $(wildcard tests/*.c)
TEST_OBJECTS = $(patsubst tests/%.c,$(OUT_TESTS)/%.o,$(TEST_FILES))
LINKER = kernel.ld
GEN_LINKER = $(OUT)/kernel.ld
PATCH_GEN = $(OUT)/create_patch
KERNEL_OLD ?= $(OUT)/kernel.old
UPDATE_BIN = $(OUT)/update.bin

# Neo config
NEO_DIR = $(CURDIR)/neo
NEO_SRC = $(NEO_DIR)/src
CONFIG_H = $(CURDIR)/include/gen/config.h
BUNDLE_IMG = $(OUT)/build_bundle.img

LIBNEO = $(NEO_DIR)/target/$(RUST_TARGET)/release/libneo.a
ALL_OBJECTS = $(OBJECTS) $(KERNEL_OBJECT) $(LIBNEO)

all: $(OUT) $(CONFIG_H) dirs $(KERNEL_OBJECT) $(LIBNEO) $(TARGET)

qemu: CFLAGS += -D__QEMU__
qemu: all

arm: all
arm64: all
riscv: all
riscv64: all
x86: all
x86_64: all

run: all
	qemu-system-riscv32 -machine virt -bios none -kernel $(TARGET) -nographic -drive file=disk.img,if=none,format=raw,id=hd0 -global virtio-mmio.force-legacy=false -device virtio-blk-device,drive=hd0

arch64: CFLAGS := $(filter-out -march=rv32imac_zicsr_zifencei -mabi=ilp32,$(CFLAGS)) -march=rv64gc -mabi=lp64d
arch64: RUST_TARGET = riscv64gc-unknown-none-elf
arch64: all

NOSHELL_TARGET = $(OUT)/kernel-noshell
noshell: NOSHELL_DIRS = $(filter-out shell,$(DIRS))
noshell: CFLAGS += -D__NOSHELL__
noshell: $(OUT) noshell_dirs $(KERNEL_OBJECT) $(LIBNEO) $(NOSHELL_TARGET)

update: all $(PATCH_GEN)
	@if [ ! -f $(KERNEL_OLD) ]; then \
		echo "Creating baseline $(KERNEL_OLD)"; \
		cp $(TARGET) $(KERNEL_OLD); \
	fi
	$(PATCH_GEN) $(KERNEL_OLD) $(TARGET) $(UPDATE_BIN)

DEBUG_TARGET = $(OUT)/kernel-debug
debug: CFLAGS += -D__DEBUG__
debug: $(OUT) dirs $(KERNEL_OBJECT) $(LIBNEO) $(DEBUG_TARGET)

tests: $(OUT) $(OUT_TESTS) dirs_with_tests $(TARGET_TESTS)

$(PATCH_GEN): scripts/c/create_patch.c | $(OUT)
	gcc -O2 $< -o $@

$(OUT):
	mkdir -p $(OUT)

$(OUT_TESTS):
	mkdir -p $(OUT_TESTS)

dirs: $(OUT)
	@for dir in $(DIRS); do $(MAKE) -C $$dir CC="$(CC)" CFLAGS="$(CFLAGS)" OUT=$(OUT) || exit 1; done

$(OBJECTS): dirs
	@true
	
noshell_dirs: $(OUT)
	@for dir in $(NOSHELL_DIRS); do $(MAKE) -C $$dir CC="$(CC)" CFLAGS="$(CFLAGS)" OUT=$(OUT) || exit 1; done

dirs_with_tests: $(OUT)
	@for dir in $(DIRS); do $(MAKE) -C $$dir CC="$(CC)" CFLAGS="$(CFLAGS)" OUT=$(OUT) || exit 1; done

$(KERNEL_OBJECT): kernel.c $(CONFIG_H) | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_TESTS)/%.o: tests/%.c | $(OUT_TESTS)
	$(CC) $(CFLAGS) -c $< -o $@

$(CONFIG_H): $(CONFIG_FILE) scripts/create_config.sh
	./scripts/create_config.sh $(CONFIG)

$(GEN_LINKER): $(LINKER) $(CONFIG_H) | $(OUT)
	sed -e 's/OUTPUT_ARCH[[:space:]]*=[[:space:]]*/OUTPUT_ARCH/g' \
	    -e 's/ENTRY[[:space:]]*=[[:space:]]*/ENTRY/g' \
	    -e 's/SECTIONS[[:space:]]*=[[:space:]]*/SECTIONS/g' \
	    -e 's/\(\.[a-zA-Z0-9_.]\+\)[[:space:]]*=[[:space:]]*{/\1 : {/g' \
	    -e 's/"riscv"/"$(LD_ARCH)"/g' \
	    -e 's/_start/$(LD_ENTRY)/g' \
	    -e 's/0x80000000/$(LD_BASE)/g' $< > $@

neo_prebuild:
	@mkdir -p $(NEO_SRC)
	@if [ -f "$(CONFIG_H)" ] && grep -q '^[[:space:]]*#define[[:space:]]\+SEED' "$(CONFIG_H)"; then \
		grep -E '^[[:space:]]*#define[[:space:]]+SEED' "$(CONFIG_H)" | awk -F'"' '{print $$2}' > "$(NEO_SRC)/seed.bin"; \
	elif [ -f "$(NEO_SRC)/seed.default" ]; then \
		cp "$(NEO_SRC)/seed.default" "$(NEO_SRC)/seed.bin"; \
	else \
		head -c 32 /dev/urandom > "$(NEO_SRC)/seed.bin"; \
	fi
	@if [ -f "/creds/key" ]; then \
		cp "/creds/key" "$(NEO_SRC)/creator.key"; \
	elif [ -f "$(CURDIR)/creds/key" ]; then \
		cp "$(CURDIR)/creds/key" "$(NEO_SRC)/creator.key"; \
	else \
		head -c 32 /dev/urandom > "$(NEO_SRC)/creator.key"; \
	fi
	@head -c 32 /dev/urandom > "$(NEO_SRC)/update.key"
	@if [ ! -f "$(NEO_SRC)/build.sig" ]; then head -c 32 /dev/zero > "$(NEO_SRC)/build.sig"; fi
	@if [ ! -f "$(NEO_SRC)/kernel.sig" ]; then head -c 32 /dev/zero > "$(NEO_SRC)/kernel.sig"; fi
	@if [ ! -f "$(NEO_SRC)/malware-list" ]; then touch "$(NEO_SRC)/malware-list"; fi

$(LIBNEO): neo_prebuild
	cargo build --manifest-path $(NEO_DIR)/Cargo.toml --release --target $(RUST_TARGET)

$(TARGET) $(DEBUG_TARGET) $(NOSHELL_TARGET): $(ALL_OBJECTS) $(GEN_LINKER)
	$(CC) $(CFLAGS) -nostdlib -T $(GEN_LINKER) -o $@ $(ALL_OBJECTS)
	@if command -v $(subst gcc,objcopy,$(firstword $(CC))) >/dev/null 2>&1; then \
		$(subst gcc,objcopy,$(firstword $(CC))) -O binary $@ $@.bin; \
	elif command -v objcopy >/dev/null 2>&1; then \
		objcopy -O binary $@ $@.bin; \
	else \
		cp $@ $@.bin; \
	fi
	@sha256sum $@.bin | awk '{print $$1}' | xxd -r -p > $(NEO_SRC)/kernel.sig
	@cp $(NEO_SRC)/kernel.sig $(OUT)/kernel.sig
	@cat $@.bin > $(BUNDLE_IMG)
	@if [ -f disk.img ]; then cat disk.img >> $(BUNDLE_IMG); fi
	@sha256sum $(BUNDLE_IMG) | awk '{print $$1}' | xxd -r -p > $(NEO_SRC)/build.sig
	@cp $(NEO_SRC)/build.sig $(OUT)/build.sig
	@echo "Signatures computed:"
	@echo "  Kernel SHA-256: $$(sha256sum $@.bin | awk '{print $$1}')"
	@echo "  Build  SHA-256: $$(sha256sum $(BUNDLE_IMG) | awk '{print $$1}')"

$(TARGET_TESTS): $(TEST_OBJECTS) $(OBJECTS) $(KERNEL_OBJECT) $(LIBNEO) $(GEN_LINKER)
	$(CC) $(CFLAGS) -nostdlib -T $(GEN_LINKER) -o $@ $(TEST_OBJECTS) $(OBJECTS) $(KERNEL_OBJECT) $(LIBNEO)

clean:
	@for dir in $(DIRS_ALL); do $(MAKE) -C $$dir clean OUT=$(OUT); done
	cargo clean --manifest-path $(NEO_DIR)/Cargo.toml
	rm -rf $(OUT)

.PHONY: all dirs noshell_dirs dirs_with_tests tests clean run debug qemu arch64 noshell arm arm64 riscv riscv64 x86 x86_64 neo_prebuild
