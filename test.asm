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
