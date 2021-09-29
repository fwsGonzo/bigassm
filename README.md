# 128-bit RISC-V assembler

RISC-V has a 128-bit ISA that is fairly developed, but not standardized fully yet.
I am maintaining a [RISC-V userspace emulator library](https://github.com/fwsGonzo/libriscv) that is written in modern C++, and the ISA is actually specified as a template parameter when instantiating the machine. So, it felt natural to explore how much work it would be to change *Machine\<RISCV64>* to *Machine\<RISCV128>*.

Turns out, not too much work. The ISA is a bit unclear on some things, but I think I've got the gist of it. Unfortunately for me, I didn't have any way of using this .. very useful architecture. So naturally I had to write an assembler for it too.

## Big assembler

The assembler is in the early stages, but it supports the most basic instructions. Enough to assemble the test assembly in the root folder, for now. The assembler is easy to expand on, but it lacks helpful hints when things are not correct. It also lacks many limit checks that would produce erroneous assembly.

It is written for C++17 (although currently does not use any fancy features, so should be C++11 compatible).

While there is no ELF format for 128-bit anything, this assembler outputs an [ELFCLASS128](src/elf128.h) file that is a could-be 128-bit ELF format. It is loadable by libriscv, specifically the [emu128 emulator project](https://github.com/fwsGonzo/libriscv/tree/master/emulator/emu128).

## Simple one-pass

The assembler currently does a one-pass through the assembly, and corrects forward labels once they appear. It also resolves sections as they appear and will give them the right RWX attributes in the ELF. This means that there may be potential inefficiencies. It is, however, very fast.

## Example

```asm
.org 0x100020003000400050006000

.section .text
.global _start
_start:             ;; Entry point label
	;; Build a 128-bit value using t1 as temporary register
	set t0, t1, 0xAAAA1111222233334444555566667770
	xor sp, sp
	add sp, t0  ;; Set stack pointer

	li  s0, 4
	xor s1, s1
repeat:
	add s0, -1
	bne s0, s1, repeat

	call my_function ;; A regular function call
	;; We return here after the function ends.

exit:
	li a0, 0x666    ;; Exit code (1st arg)
	syscall 1       ;; Execute system call 1 (exit)
	jmp exit        ;; Loop exit to prevent problems

.section .rodata
.readonly
hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string

.section .text
my_function:
	add sp, -32
	sq a0, sp+0     ;; Save A0

	li t0, 2        ;; Syscall 2 (print)
	sw t0, sp+16    ;; Store 32-bit value
	lw sp+16, a7    ;; Load 32-bit value

	la a0, hello_world ;; address of string
	ecall           ;; Execute syscall

	lq sp+0, a0     ;; Restore A0

	ret
```

The store is pretty useless, but it shows how to do a SP-relative store.


## Instructions and pseudo-instructions

- my_label:
	- Create a new label named 'my_label' which can be jumped to.
- li [dst], constant
	- Loads integer constant into register 'dst'.
- set [dst], [tmp], constant
	- Loads up to 128-bit constant into 'dst' using 'tmp' as intermediate register. Uses many instructions.
- la [dst], label
	- Loads address at label into register 'dst'. Supports relative 128-bit addresses if the label is +/-2GB. Supports absolute +/- 2GB address.
- lq [dst], [reg]+offset
	- Load 128-bit value from [reg]+offset memory address.
	- Other sizes: lb (8-bit), lh (16-bit), lw (32-bit), ld (64-bit).
	- Unsigned: lbu (8-bit), lhu (16-bit), lwu (32-bit), ldu (64-bit).
- sq [reg]+offset, [dst]
	- Store 128-bit value into [reg]+offset memory address.
	- Other sizes: sb (8-bit), sh (16-bit), sw (32-bit), sd (64-bit).
- call label
	- Make a _function call_ to 'label' which can be returned from. Uses PC-relative addressing.
- farcall [tmp], label
	- Make a _function call_ to a far away 'label' which can be returned from. Register 'tmp' is used to build the full address.
- ret
	- Return back from any _function call_.
- jmp label
	- Jump directly to label.
- syscall [constant]
	- Puts constant into A7 and performs system call. Arguments in A0-A6.
- ecall
	- Perform system call from register A7. Arguments in A0-A6.
- ebreak
	- Debugger breakpoint.
- wfi
	- Wait for interrupts (stops the machine).

Branches:

- beq [r1] [r2] label
	- Jump when r1 and r2 is equal.
- bne [r1] [r2] label
	- Jump when r1 and r2 is _not_ equal.
- blt [r1] [r2] label
	- Jump when r1 is less than to r2.
- bge [r1] [r2] label
	- Jump when r1 is greater or equal to r2.
- bltu [r1] [r2] label
	- Jump when _unsigned_ r1 is less than _unsigned_ r2.
- bgeu [r1] [r2] label
	- Jump when _unsigned_ r1 is greater or equal to _unsigned_ r2.

Arithmetic and logical operations:

- add, sll, slt, sltu, srl, and, or, xor [dst] [reg _or_ imm]
	- Operation on register with register or immediate.
	- For 32-bit operations append w to the instruction. For example add becomes addw.
	- For 64-bit operations append d to the instruction. For example xor becomes xord.

- sub, mul, div, divu, rem, remu [dst] [reg]
	- Subtraction, multiplication, division, unsigned division, remainder, unsigned remainder.
	- Operation on register with register.
	- As above, append w for 32-bit and d for 64-bit.

Complete [list of available instructions](src/opcodes.cpp).

## Pseudo-ops

- db, dh, dw, dd, dq [constant]
	- Insert aligned constant of 8-, 16-, 32-, 64- or 128-bits into current position.
- resb, resh, resw, resd, resq [times]
	- Reserve aligned 1, 2, 4, 8 or 16 bytes multiplied by constant.
- incbin "file.name"
	- Inserts binary data taken from filename at current position.

Complete [list of available pseudo-ops](src/pseudo_ops.cpp).

## Directives

- .org 0x10000
	- Set the base address of the binary, which now starts at 0x10000. Supports 128-bit addresses.
- .align 4
	- Align memory to the given power-of-two.
- .endfunc name
	- Calculates and sets the size of 'name' and type to function.
- .execonly
	- Make the section execute-only. (ELF only)
- .finish_labels
	- Output any labels that aren't directly attached to data, or force outputting a label before alignment.
- .include filename
	- Read contents from file, parse and assemble it at the current position.
- .readonly
	- Make the section read-only. (ELF only)
- .section name
	- Create or continue an ELF section. Some attributes are automatically applied based on the data put into the section.
- .size label
	- Calculate the difference between the current position and the given label and output a 32-bit constant.
- .string "String here!"
	- Insert a zero-terminated string.

Complete [list of available directives](src/directive.cpp).
