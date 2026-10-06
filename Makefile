CC = gcc
#CFLAGS =
OUT = $(CURDIR)/out
OUT_TESTS = $(OUT)/tests
TARGET = $(OUT)/kernel
TARGET_TESTS = $(OUT_TESTS)/run_tests
SRC_DIR = src
VPATH = $(SRC_DIR)
DIRS = assembly clock screen tools utils
OBJECTS = $(foreach dir,$(DIRS),$(OUT)/$(dir).o)
MAIN_KERNEL_FILES = $(notdir $(wildcard $(SRC_DIR)/*.c))
MAIN_KERNEL_OBJECTS = $(patsubst %.c,$(OUT)/%.o,$(MAIN_KERNEL_FILES))
ALL_OBJECTS = $(OBJECTS) $(MAIN_KERNEL_OBJECTS)
TEST_FILES = $(wildcard tests/*.c)
TEST_OBJECTS = $(patsubst tests/%.c,$(OUT_TESTS)/%.o,$(TEST_FILES))

all: $(OUT) dirs $(TARGET)

tests: $(OUT) $(OUT_TESTS) dirs_with_tests $(TARGET_TESTS)

$(OUT):
	mkdir -p $(OUT)

$(OUT_TESTS):
	mkdir -p $(OUT_TESTS)
	
dirs: $(OUT)
	@for dir in $(DIRS); do $(MAKE) -C $$dir OUT=$(OUT); done

dirs_with_tests: $(OUT)
	@for dir in $(DIRS); do $(MAKE) -C $$dir OUT=$(OUT); done

$(OUT)/%.o: %.c | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_TESTS)/%.o: tests/%.c | $(OUT_TESTS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(ALL_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJECTS)

$(TARGET_TESTS): $(TEST_OBJECTS) $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	@for dir in $(DIRS); do $(MAKE) -C $$dir clean; done
	rm -rf $(OUT)

.PHONY: all dirs dirs_with_tests tests clean

