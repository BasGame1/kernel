# Main makefile
CC=gcc
#CFLAGS = 
OUT = $(CURDIR)/out
TARGET = $(OUT_DIR)/kernel
# include/ no
DIRS = assembly clock screen tests tools utils
OBJECTS = $(foreach dir,$(DIRS),$(OUT)/$(dir).o)
MAIN_KERNEL_FILES = $(wildcard *.c)
MAIN_KERNEL_OBJECTS = $(patsubst %.c,$(OUT)/%.o,$(MAIN_KERNEL_FILES))
ALL_OBJECTS = $(OBJECTS) $(MAIN_KERNEL_OBJECTS)

all: $(OUT) subdirs $(TARGET)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)
	
dirs: $(OUT)
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir OUT=$(OUT); \
	done
	
$(OUT)/%.o: %.c | $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(ALL_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OUT)/*.o

clean:
	@for dir in $(DIRS); do $(MAKE) -C $$dir clean; done
	rm -rf $(OUT)

.PHONY: all dirs clean
