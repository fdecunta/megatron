CC    = cc
FLAGS = -Wall -Wextra -Werror -Wconversion

PROG = megatron
SRC = megatron.c

$(PROG): $(SRC)
	$(CC) $(FLAGS) $(SRC) -o $(PROG)

run: $(PROG)
	./$(PROG) -d fake_filmoteca

debug: $(SRC)
	$(CC) -DDEBUG $(FLAGS) $(SRC) -o $(PROG)

clean: 
	rm -f $(PROG)

mem: $(PROG)
	valgrind --leak-check=full ./$(PROG) -d fake_filmoteca

.PHONY: run clean mem debug
