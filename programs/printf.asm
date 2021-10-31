.section .rodata
.readonly
hexlut:
	.ascii "0123456789abcdef"

.section .text
snprint:
	add sp, -16
	;; a0 buffer
	;; a1 buflen
	;; a2 fmt string
	;; a3 fmt args

	mv t0, a0      ;; t0 start of buffer
	add t1, a0, a1 ;; t1 end of buffer
	mv t2, a2      ;; t2 <- fmt

	;; iterate fmt string
snprint_iterate_fmt:
	lb t3, t2   ;; t3 <- *fmt
	inc t2      ;; t2++

	;; copy character (for now)
	sb t0, t3
	inc t0

	;; iterate until *fmt == zero
	beq t3, zero, snprint_end

	jmp snprint_iterate_fmt

	;; return when buffer >= end
	bge t0, t1, snprint_end

snprint_end:
	;; calculate buffer length
	sub a0, t0, a0 ;; a0 = t0 - a0
	ret
.endfunc snprint
