#!/usr/bin/env bash
set -e

pushd build
ninja
popd
./build/fab128 test.asm test.bin
riscv64-linux-gnu-objdump -b binary -m riscv -EL -D test.bin
