CC    = cc
FLAGS = -Wall -Wextra -Werror -Wconversion

PROG = megatron
SRC = megatron.c

BIN = /usr/local/bin

$(PROG): $(SRC)
	$(CC) $(FLAGS) $(SRC) -o $(PROG)

run: $(PROG)
	./$(PROG) -d fake_filmoteca

install: $(PROG)
	cp $(PROG) $(BIN)

remove:
	rm -f $(BIN)/$(PROG)

clean: 
	rm -f $(PROG)

mem: $(PROG)
	valgrind --leak-check=full ./$(PROG) fake_filmoteca

.PHONY: run clean mem debug
