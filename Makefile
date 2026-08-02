CC    = cc
FLAGS = -pedantic -Wall -Wextra -Werror -Wconversion
LIBS  =

PROG = megatron
SRC = megatron.c

$(PROG): $(SRC)
	$(CC) $(FLAGS) $(SRC) $(LIBS) -o $(PROG)

run: $(PROG)
	./$(PROG) -d fake_filmoteca

mem: $(PROG)
	valgrind --leak-check=full ./$(PROG) -d fake_filmoteca
