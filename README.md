# 128-bit RISC-V assembler

RISC-V has a 128-bit ISA that is fairly developed, but not standardized fully yet.
I am maintaining a [RISC-V userspace emulator library](https://github.com/fwsGonzo/libriscv) that is written in modern C++, and the ISA is actually specified as a template parameter when instantiating the machine. So, it felt natural to explore how much work it would be to change *Machine\<RISCV64>* to *Machine\<RISCV128>*.

Turns out, not too much work. The ISA is a bit unclear on some things, but I think I've got the gist of it. Unfortunately for me, I didn't have any way of using this .. very useful architecture. So naturally I had to write an assembler for it too.

## rv128fab

The assembler is in the early stages, but it supports the most basic instructions. Enough to assemble the test assembly in the root folder, for now. The assembler is easy to expand on, but it lacks helpful hints when things are not correct. It also lacks many limit checks that would produce erroneous assembly.

It is written for C++17 (although currently does not use any fancy features, so should be C++11 compatible).

Since there is no ELF format for 128-bit anything, it produces raw binaries. The assembler currently assumes that the program base is `0x100000`.

## Simple one-pass

The assembler currently does a one-pass through the assembly, and corrects forward labels once they appear. This means that there may be potential inefficiencies. It is, however, very fast.

## Example

```asm
.global _start
_start:             ;; Entry point label
	li sp, -8       ;; Stack at end of 128-bit
	sq a7, sp +0    ;; Store 128-bit value

	li a7, 2        ;; Syscall 2 (print)
	la a0, hello_world ;; address of string
	scall           ;; Execute syscall

	li a7, 1        ;; Syscall 1 (exit)
	li a0, 0        ;; Code: 0
	scall           ;; Execute syscall

hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string
```

The store is pretty useless, but it shows how to do a SP-relative store.
