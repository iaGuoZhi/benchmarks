MODE?=RELEASE
#MODE?=DEBUG

ifeq ($(MODE), DEBUG)
CFLAG=-g -fsanitize=address
else
CFLAG=-O3 -DNDEBUG
endif
CFLAG += -Iinc -I. -fPIC

BITSTREAM := $(shell ls inc/BitStream/*)

LIBS= lib/libcompress.a lib/libdecompress.a

all: compression_test

.PHONY: clean

lib/libcompress.a: $(COMPRESS_SRC)
	g++ -c -g Compress/CombCompressor.cpp -o encode.o $(CFLAG)
	g++ -c -g Compress/Eraser.cpp -o eraser.o $(CFLAG)
	g++ -c -g Compress/GorillaXORCompressor.cpp -o gorilla_comp.o $(CFLAG)
	g++ -c -g Compress/ChimpXORCompressor.cpp -o chimp_comp.o $(CFLAG)
	g++ -c -g Compress/ElfXORCompressor.cpp -o elf_comp.o $(CFLAG)
	g++ -c -g Compress/TimestampCompressor.cpp -o ts_comp.o $(CFLAG)
	g++ -c -g -o utils.o utils.cpp $(CFLAG)
	ar -rcs $@ encode.o eraser.o gorilla_comp.o chimp_comp.o elf_comp.o ts_comp.o utils.o
	rm encode.o eraser.o gorilla_comp.o chimp_comp.o elf_comp.o ts_comp.o utils.o

lib/libdecompress.a: $(DECOMPRESS_SRC)
	g++ -c -g Decompress/CombDecompressor.cpp -o decode.o $(CFLAG)
	g++ -c -g Decompress/Recover.cpp -o recover.o $(CFLAG)
	g++ -c -g Decompress/GorillaXORDecompressor.cpp -o gorilla_decom.o $(CFLAG)
	g++ -c -g Decompress/ChimpXORDecompressor.cpp -o chimp_decom.o $(CFLAG)
	g++ -c -g Decompress/ElfXORDecompressor.cpp -o elf_decom.o $(CFLAG)
	g++ -c -g Decompress/TimestampDecompressor.cpp -o ts_decom.o $(CFLAG)
	ar -rcs $@ decode.o recover.o gorilla_decom.o chimp_decom.o elf_decom.o ts_decom.o
	rm decode.o recover.o gorilla_decom.o chimp_decom.o elf_decom.o ts_decom.o

compression_test.o: compression_test.cpp
	g++ -c -g ${CFLAG} -o $@ $<

compression_test: compression_test.o $(LIBS)
	g++ -g -o $@ compression_test.o $(CFLAG) \
		-Llib -lcompress -ldecompress

clean:
	rm -f *.o tmp.cmp compression_test
	rm -f $(LIBS)
