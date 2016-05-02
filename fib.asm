; fib.asm
; 不适合三位数
section .data
	num:	db 0
	sum:	dd 0
	offset:	dd 0
	counter:	db 0
	temp:	db 0
	line:	db 0ah	;换行符
	pdigit:	db 0
	rdigit:	db 0
	;颜色
	color1:	db 1Bh, '[31;1m', 0
	.len equ $-color1
	color2: db 1Bh, '[34;1m', 0
	.len equ $-color2
	color3:	db 1Bh, '[37;1m', 0
	.len equ $-color3

section .bss
	arrayx:	resb 20
	array:	resd 50
section .text
	global _start
_start:
	call getnumbers
	call printnumbers
	call exit

; 打印单个数字
printdigit:
	PUSHA
	;修改字体颜色
	movzx ax, byte[counter]
	mov bl, 3
	div bl
	cmp ah, 0
	JE	col1
		cmp	ah, 1
			JE	col2
				JMP	col3
	col1:
		mov eax, 4
		mov	ebx, 1
		mov	ecx, color1
		mov	edx, color1.len
		int 80h
		JMP continue
	col2:
		mov eax, 4
		mov	ebx, 1
		mov	ecx, color2
		mov	edx, color2.len
		int 80h
		JMP continue
	col3:
		mov eax, 4
		mov	ebx, 1
		mov	ecx, color3
		mov	edx, color3.len
		int 80h
		JMP continue
	continue:
		mov eax, 4
		mov ebx, 1
		mov ecx, pdigit
		mov edx, 1
		int 80h
		mov eax, 4
		mov ebx, 1
		mov ecx, color3
		mov edx, color3.len
		int 80h
		POPA
ret

changeline:
	PUSHA
	mov eax, 4
	mov	ebx, 1
	mov	ecx, line
	mov	edx, 1
	int 80h
	POPA
ret

readdigit:
	PUSHA
	mov eax, 3
	mov ebx, 0
	mov ecx, rdigit
	mov edx, 1
	int 80h
	POPA
ret

exit:
	mov ebx, 0
	mov eax, 1
	int 80h

getnumbers:
	call readdigit
	cmp	byte[rdigit], 0ah
	JE	if
		JMP	L1
	if	call cal
		;存储结果到数组
		call save
		mov dword[offset], 0
		ret
	L1:
		cmp	byte[rdigit], 20h
		JNE	L2
			call cal
			;存储结果到数组
			call save
			JMP	getnumbers
		L2:	
			mov al, byte[num]
			mov bl, 10
			mul bl
			sub	byte[rdigit], 30h
			movzx	bx, byte[rdigit]
			add ax, bx
			mov byte[num], al
			JMP getnumbers

printnumbers:
	DEC	byte[counter]	;此时counter为剩下需要打印的数量
	call load	;将数字导入sum地址中
	call printanumber
	call changeline
	cmp	byte[counter], 0
	JE	if2
		JMP	printnumbers
	if2:
		mov dword[offset], 0
		ret

cal:
	mov	eax, 1
	mov	ebx, 0
	for:
		mov ecx, eax
		add eax, ebx
		mov ebx, ecx
		DEC	byte[num]
		cmp byte[num], 0
		JE	S1
			JMP for
		S1:	mov dword[sum], ebx	
ret

;将sum存入数组中
save:
	INC	byte[counter]
	mov	eax, array
	add	eax, dword[offset]
	mov	ebx, dword[sum]
	mov	dword[eax], ebx
	add	dword[offset], 4
ret

;将数组中的数取出到sum
load:
	mov eax, array
	add	eax, dword[offset]
	mov	ebx, dword[eax]
	mov	dword[sum], ebx
	add	dword[offset], 4
ret

;此时打印存在sum中的数
printanumber:
	mov	byte[temp], 0
	mov	eax, dword[sum]
	split:
		mov edx, 0
		mov	ebx, 10
		div	ebx
		mov	ecx, arrayx
		movzx	ebx, byte[temp]
		add	ecx, ebx
		mov	byte[ecx], dl
		add	byte[temp], 1
		cmp	eax, 0	;eax为商,edx为余数
		JE	print
			JMP	split
		print:
			sub	byte[temp], 1
			mov	eax, arrayx
			movzx	ebx, byte[temp]
			add	eax, ebx
			mov	bl, byte[eax]
			mov	byte[pdigit], bl
			add	byte[pdigit], 30h
			call printdigit
			cmp	byte[temp], 0
			JE  if3
				JMP	print
			if3:
				ret