# Assembly makefile
#ASFLAGS =
SRC = $(wildcard *.c)
OBJECTS = $(SRC:.c=.o)
COMBINED_OBJECTS = $(OUT_DIR)/$(notdir $(CURDIR)).o

all: $(COMBINED_OBJECTS)

%.o: %.c
	$(AS) $(ASFLAGS) -c $< -o out/$@

$(COMBINED_OBJECTS): $(OBJ)
	$(AS) -r -o $@ $^
	
clean:
     rm -rf $(OBJECTS) $(COMBINED_OBJECTS)
