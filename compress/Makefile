MODE?=RELEASE
#MODE?=DEBUG

ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
else
CFLAG=-O3 -DNDEBUG
endif
CFLAG += -Iinc -fPIC

BITSTREAM := $(shell ls inc/BitStream/*)

LIBS= lib/libgorilla.a lib/libchimp.a lib/libelf.a lib/libcomb.a

all: compression_test

.PHONY: clean

lib/libgorilla.a: Gorilla/gorilla.cpp $(BITSTREAM)
	g++ -c -g -o gorilla.o $< $(CFLAG)
	ar -rcs lib/libgorilla.a gorilla.o
	rm gorilla.o

lib/libchimp.a: $(CHIMP_SRC)
	g++ -c -g -o encode.o Chimp/ChimpEncode.cpp $(CFLAG)
	g++ -c -g -o decode.o Chimp/ChimpDecode.cpp $(CFLAG)
	ar -rcs $@ encode.o decode.o
	rm encode.o decode.o

lib/libelf.a: $(ELF_SRC)
	g++ -c -g -o encode.o Elf/ElfCompressor.cpp $(CFLAG)
	g++ -c -g -o decode.o Elf/ElfDecompressor.cpp $(CFLAG)
	g++ -c -g -o utils.o Elf/utils.cpp $(CFLAG)
	ar -rcs $@ encode.o decode.o utils.o
	rm encode.o decode.o utils.o

lib/libcomb.a: $(ELF_SRC)
	#g++ -c -g -o encode.o Comb/CombCompress.cpp $(CFLAG)
	g++ -c -g Comb/CombCompressor.cpp -o encode.o $(CFLAG)
	g++ -c -g Comb/EraserCompressor.cpp -o eraser.o $(CFLAG)
	g++ -c -g Comb/GorillaXORCompressor.cpp -o gorilla_comp.o $(CFLAG)
	g++ -c -g Comb/ChimpXORCompressor.cpp -o chimp_comp.o $(CFLAG)
	g++ -c -g Comb/ElfXORCompressor.cpp -o elf_comp.o $(CFLAG)
	g++ -c -g -o decode.o Comb/CombDecompressor.cpp $(CFLAG)
	g++ -c -g Comb/RecoverDecompressor.cpp -o recover.o $(CFLAG)
	g++ -c -g Comb/GorillaXORDecompressor.cpp -o gorilla_decom.o $(CFLAG)
	g++ -c -g Comb/ChimpXORDecompressor.cpp -o chimp_decom.o $(CFLAG)
	g++ -c -g Comb/ElfXORDecompressor.cpp -o elf_decom.o $(CFLAG)
	g++ -c -g -o utils.o Comb/utils.cpp $(CFLAG)
	ar -rcs $@ encode.o decode.o eraser.o recover.o gorilla_comp.o chimp_comp.o elf_comp.o gorilla_decom.o chimp_decom.o elf_decom.o utils.o
	rm encode.o decode.o eraser.o recover.o gorilla_comp.o chimp_comp.o elf_comp.o gorilla_decom.o chimp_decom.o elf_decom.o utils.o

compression_test.o: compression_test.cpp
	g++ -c -g ${CFLAG} -o $@ $<

compression_test: compression_test.o $(LIBS)
	g++ -g -o $@ compression_test.o $(CFLAG) \
		-Llib -lgorilla -lchimp -lelf -lcomb

clean:
	rm -f *.o tmp.cmp compression_test
	rm -f $(LIBS)
