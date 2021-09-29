.org 0x100020003000400050006000

.section .text
.global _start
_start:             ;; Entry point label
	li a0, 0x666    ;; Exit code (1st arg)
	syscall 1       ;; Execute system call 1 (exit)
.endfunc _start
