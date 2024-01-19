MODE?=RELEASE

ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
else
CFLAG=-O3 -DNDEBUG
endif
CFLAG += -Iinc -fPIC

BITSTREAM := $(shell ls inc/BitStream/*)

LIBS= lib/libgorilla.a lib/libchimp.a lib/libelf.a

all: compression_test

.PHONY: clean

lib/libgorilla.a: Gorilla/gorilla.cpp $(BITSTREAM)
	g++ -c -o gorilla.o $< $(CFLAG)
	ar -rcs lib/libgorilla.a gorilla.o
	rm gorilla.o

lib/libchimp.a: $(CHIMP_SRC)
	g++ -c -o encode.o Chimp/ChimpEncode.cpp $(CFLAG)
	g++ -c -o decode.o Chimp/ChimpDecode.cpp $(CFLAG)
	ar -rcs $@ encode.o decode.o
	rm encode.o decode.o

lib/libelf.a: $(ELF_SRC)
	g++ -c -o encode.o Elf/ElfCompressor.cpp $(CFLAG)
	g++ -c -o decode.o Elf/ElfDecompressor.cpp $(CFLAG)
	g++ -c -o utils.o Elf/utils.cpp $(CFLAG)
	ar -rcs $@ encode.o decode.o utils.o
	rm encode.o decode.o utils.o

compression_test.o: compression_test.cpp
	g++ -c ${CFLAG} -o $@ $<

compression_test: compression_test.o $(LIBS)
	g++ -o $@ compression_test.o $(CFLAG) \
		-Llib -lgorilla -lchimp -lelf

clean:
	rm -f *.o tmp.cmp compression_test
	rm -f $(LIBS)
