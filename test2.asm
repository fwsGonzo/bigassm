.section .rodata
.readonly
hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string
hello_world_size:
	.size hello_world
