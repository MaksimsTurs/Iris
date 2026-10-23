CC		 = clang
CFLAGS = -Wfloat-equal \
				 -Wunreachable-code \
				 -Wstrict-prototypes \
				 -Wswitch-default \
				 -Wswitch-enum \
				 -Wextra \
				 -Wall \
				 -Wshadow \
				 -Wundef \
				 -Wconversion
CFLAGS += -ggdb -O0
CSRC		= main.c lexer.c parser.c bytecode.c vm.c utils.c mem.c

all:
	$(CC) $(CFLAGS) -I./includes $(CSRC) -o main
