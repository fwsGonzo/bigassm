.section .rodata
.readonly
hello_world:        ;; String label
	.type hello_world, object
	.string "Hello World!" ;; Zt-string
hello_world_size:
	.type hello_world_size, object
	.size hello_world
