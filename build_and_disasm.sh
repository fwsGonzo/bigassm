#!/usr/bin/env bash
set -e
ASMF=${1:-programs/test.asm}
OUTELF=${2:-test.elf}

mkdir -p build
pushd build
cmake .. -G Ninja
ninja
popd
time ./build/fab128 $ASMF $OUTELF
riscv64-linux-gnu-objdump -b binary -m riscv -EL -D $OUTELF.bin
