section .data
	;颜色
	color:	db 1Bh, '[31;1m', 0
	.len equ $-color
	a:	db 1Bh, '[37;1m', 0
	.len equ $-a
	b:	db 0
section .text
	global print
print:
	mov	eax, [esp+4]
	mov	ecx, [esp+8]
	mov	edx, [esp+12]
	cmp	edx, 0
	JE	change
continue:
		mov	bl, byte[eax]
		mov	byte[b], bl
		cmp	ecx, 0
		JNE	printByte
			mov eax, 4
			mov ebx, 1
			mov ecx, a
			mov edx, a.len
			int 80h
ret

printByte:
	PUSHA
	mov	eax, 4
	mov	ebx, 1
	mov	ecx, b
	mov	edx, 1
	int 80h
	POPA
	sub	ecx, 1
	add	eax, 1
	JMP	continue

change:
	PUSHA
	mov eax, 4
	mov	ebx, 1
	mov	ecx, color
	mov	edx, color.len
	int 80h
	POPA
	JMP	continue

