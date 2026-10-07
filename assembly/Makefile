CC ?= gcc
CFLAGS ?=

SRC = boot.S trap.S
OBJECTS = boot.o trap.o
COMBINED_OBJECTS = $(OUT)/$(notdir $(CURDIR)).o

all: $(COMBINED_OBJECTS)

boot.o: boot.S
	sed -e 's/\.[[:space:]]\+/./g' $< | $(CC) $(CFLAGS) -c -x assembler - -o $@

trap.o: trap.S
	$(CC) $(CFLAGS) -c $< -o $@

$(COMBINED_OBJECTS): $(OBJECTS)
	$(CC) $(CFLAGS) -nostdlib -r -o $@ $^
	rm -f $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(COMBINED_OBJECTS)

