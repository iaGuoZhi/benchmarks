# Don't compile large_elf as static
# If use g++, some error will occur due to the file is too large
clang ./large_elf.cpp -o large_elf
