cd ../code/fpzip

# Build fpzip
if [! -d "build"]; then
    mkdir build
fi
cd build
cmake ..
cmake --build . --config Release

# Build pFPC
cd ../../pFPC
gcc -O3 -pthread -std=c99 pFPC_block.c -o pfpcb
gcc -O3 -pthread -std=c99 pFPC.c -o pfpc

# Build SPFP
cd ../SPFP
gcc -O3 SPDP_block.c -o spdpb
gcc -O3 SPDP_11.c -o spdp
