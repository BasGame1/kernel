# Tools makefile
CC=gcc
#CFLAGS =
SRC = $(wildcard *.c)
OBJECTS = $(SRC:.c=.o)
COMBINED_OBJECTS = $(OUT_DIR)/$(notdir $(CURDIR)).o

all: $(COMBINED_OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o out/$@

$(COMBINED_OBJECTS): $(OBJ)
	$(CC) -r -o $@ $^
	
clean:
     rm -rf $(OBJECTS) $(COMBINED_OBJECTS)
