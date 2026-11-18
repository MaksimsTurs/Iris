EXE			:= irisc
CC		 	:= clang
CFLAGS 	:= -Wfloat-equal \
				 	 -Wunreachable-code \
				 	 -Wstrict-prototypes \
				 	 -Wswitch-default \
				 	 -Wswitch-enum \
				 	 -Wextra \
				 	 -Wall \
				 	 -Wshadow \
				 	 -Wundef \
				 	 -Wconversion
CFLAGS	+= -I.
CFLAGS 	+= -static -march=native -mtune=native
COBJS	 	:= ./bin/objs/main.o \
					 ./bin/objs/iris.o \
					 ./bin/objs/mem.o \
					 ./bin/objs/utils.o \
					 ./bin/objs/lexer.o \
					 ./bin/objs/parser.o \
					 ./bin/objs/bytecode.o \
					 ./bin/objs/vm.o \

ifeq ($(Type), Debug)
	CFLAGS += -O0 -ggdb
endif
ifeq ($(Type), Release)
	CFLAGS += -O2 -flto=full
endif

all: clean preparedirs libs irisc

preparedirs:
	mkdir bin -p
	mkdir bin/objs -p

libs:
	cd std && make && cd ..

irisc: $(COBJS)
	$(CC) $(CFLAGS) -o $(EXE) $(COBJS)
	mv irisc bin/irisc
ifeq ($(Type), Release)
	strip --strip-all bin/irisc
endif

bin/objs/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf bin
	rm -rf bin/irisc
