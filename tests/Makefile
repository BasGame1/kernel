CC = gcc
#CFLAGS =

SRC = $(wildcard *.c)
OBJECTS = $(SRC:.c=.o)
TARGET = tests

all: $(COMBINED_OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $(TARGET)
	rm -f $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(TARGET)

