.org 0xA000B000C000F000

.section .data
buffer:
	resb 512
buffer_size:
	.size buffer

.section .rodata
format_string:
	.string "Hello World\n"

.section .text
.global _start
	.type _start, function
_start:             ;; Entry point label
	li sp, 0xF000

	laq a0, t0, buffer
	ebreak

	li a1, 512      ;; Fixme
	laq a2, t0, format_string
	li a3, 0        ;; Arg 0
	call snprint

	;; Print the buffer
	mv a1, a0           ;; a1: bytes
	laq a0, t0, buffer  ;; a0: buffer
	syscall 2

.endfunc _start

exit:
	xor a0, a0
	syscall 1
	jmp exit
.endfunc exit

.include "printf.asm"

label_at_end:
