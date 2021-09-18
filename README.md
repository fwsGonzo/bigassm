
# 128-bit RISC-V assembler

RISC-V has a 128-bit ISA that is fairly developed, but not standardized fully yet.
I am maintaining a [RISC-V userspace emulator library](https://github.com/fwsGonzo/libriscv) that is written in modern C++, and the ISA is actually specified as a template parameter when instantiating the machine. So, it felt natural to explore how much work it would be to change *Machine\<RISCV64>* to *Machine\<RISCV128>*.

Turns out, not too much work. The ISA is a bit unclear on some things, but I think I've got the gist of it. Unfortunately for me, I didn't have any way of using this .. very useful architecture. So naturally I had to write an assembler for it too.

## Big assembler

The assembler is in the early stages, but it supports the most basic instructions. Enough to assemble the test assembly in the root folder, for now. The assembler is easy to expand on, but it lacks helpful hints when things are not correct. It also lacks many limit checks that would produce erroneous assembly.

It is written for C++17 (although currently does not use any fancy features, so should be C++11 compatible).

Since there is no ELF format for 128-bit anything, it produces raw binaries.

## Simple one-pass

The assembler currently does a one-pass through the assembly, and corrects forward labels once they appear. This means that there may be potential inefficiencies. It is, however, very fast.

## Example

```asm
.org 0x100000

.global _start
_start:             ;; Entry point label
	li sp, -16      ;; Stack at end of 128-bit

	li t0, 2        ;; Syscall 2 (print)
	sq t0, sp-0     ;; Store 128-bit value
	lq sp+0, a7     ;; Load 128-bit value

	la a0, hello_world ;; address of string
	scall           ;; Execute syscall

exit:
	li a7, 1        ;; Syscall 1 (exit)
	li a0, 0x666    ;; Exit code (1st arg)
	scall           ;; Execute syscall
	jmp exit

hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string
```

The store is pretty useless, but it shows how to do a SP-relative store.


## Instructions and pseudo-instructions

- my_label:
	- Create a new label named 'my_label' which can be jumped to.
- li [reg], constant
	- Loads integer constant into register.
- la [reg], label
	- Loads address at label into register.
- lq [reg], [reg]+offset
	- Load 128-bit value from [reg]+offset memory address.
- sq [reg]+offset, [reg]
	- Store 128-bit value into [reg]+offset memory address.
- jmp label
	- Jump directly to label.
- scall
	- Perform system call from register A7. Arguments in A0-A6.
- ebreak
	- Debugger breakpoint.
- wfi
	- Wait for interrupts (stops the machine).

## Pseudo-ops

- db, dh, dw, dd, dq [constant]
	- Insert aligned constant of 8-, 16-, 32-, 64- or 128-bits into current position.
- resb, resh, resw, resd, resq [times]
	- Reserve aligned 1, 2, 4, 8 or 16 bytes multiplied by constant.
- incbin "file.name"
	- Inserts binary data taken from filename at current position.

## Directives

- .org 0x1000
	- Set the base address of the binary, which now starts at 0x1000.
- .align 4
	- Align memory to the given power-of-two.
- .string "String here!"
	- Insert a zero-terminated string.

