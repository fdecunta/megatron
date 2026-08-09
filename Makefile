CC    = cc
FLAGS = -Wall -Wextra -Werror -Wconversion

PROG = megatron
SRC  = megatron.c

BIN  = /usr/local/bin

$(PROG): $(SRC)
	$(CC) $(FLAGS) $(SRC) -o $(PROG)

install: $(PROG)
	cp $(PROG) $(BIN)

remove:
	rm -f $(BIN)/$(PROG)

.PHONY: install remove 
