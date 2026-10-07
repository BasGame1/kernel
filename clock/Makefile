CC ?= gcc
CFLAGS ?=

SRC = $(wildcard *.c)
OBJECTS = $(SRC:.c=.o)
COMBINED_OBJECTS = $(OUT)/$(notdir $(CURDIR)).o

all: $(COMBINED_OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(COMBINED_OBJECTS): $(OBJECTS)
	$(CC) $(CFLAGS) -nostdlib -r -o $@ $^
	rm -f $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(COMBINED_OBJECTS)


