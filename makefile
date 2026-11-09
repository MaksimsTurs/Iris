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
CFLAGS += -static -march=native -mtune=native
CFLAGS += -O0 -ggdb
# CFLAGS += -O3 -flto
# CFLAGS += -fno-math-errno -funroll-loops
CSRC		= ./src/main.c ./src/iris.c 
CSRC	 += ./src/lexer.c 
CSRC	 += ./src/parser.c 
CSRC	 += ./src/mem.c ./src/utils.c
EXE			= irisc

all:
	$(CC) $(CFLAGS) $(CSRC) -I./ -o $(EXE)
