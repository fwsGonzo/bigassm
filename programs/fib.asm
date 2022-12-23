.org 0x100020003000400050006000

.section .text
.global _start
_start:
	li  a5, 256000000 ;; n
	li  a4, 0       ;; acc
	li  a3, 1       ;; prev
	;; Done if a5 == 0 at the start
	bne a5, zero, begin
	jmp done
continue:
	mv  a4, a0      ;; acc
begin:
	add a5, -1
	;; Accumulate next into a0
	add a0, a4, a3
	mv  a3, a4      ;; prev
	bne a5, zero, continue

done:
	syscall 1
