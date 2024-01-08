cd ./code/fpzip

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
cd ../SPDP
gcc -O3 SPDP_block.c -o spdpb
gcc -O3 SPDP_11.c -o spdp

# Build ndzip
cd ../ndzip
whichgcc=$(which gcc)
whichgpp=$(which g++)
boostdir=/usr/include/boost
whichnvcc=/opt/cuda/bin/nvcc
cmake -B build -DCMAKE_CUDA_ARCHITECTURES=75 \
	-DCMAKE_BUILD_TYPE=Release \
	-DZIP_WITH_MT=YES \
	-DNDZIP_WITH_CUDA=YES \
	-DCMAKE_CXX_FLAGS="-march=native" \
	-DCMAKE_C_COMPILER=$whichgcc \
	-DCMAKE_CXX_COMPILER=$whichgpp \
	-DCMAKE_CUDA_COMPILER=$whichnvcc \
	-DBoost_INCLUDE_DIR=$boostdir/include \
	-DBoost_LIBRARY_DIRS=$boostdir/lib
cmake --build build -j
