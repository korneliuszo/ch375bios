.8086

_TEXT  segment word public 'CODE'
assume  cs:_TEXT
dw 0AA55h
db 16
public entry
entry:
	jmp short install_irq

install_irq label near
	pushf
	push ds
	push es
	push ax
	mov ax, 0
	mov ds, ax ; load first page
	mov ah, 0x02
	int 0x16
	and al, 0x04
	jnz SHORT SkipRomInitialization

	les	ax, ds:[19h*4]

	mov WORD PTR ds:[019h*4], offset _TEXT:init
	mov WORD PTR ds:[019h*4+2], cs

	mov	WORD PTR ds:[0b5h*4], ax
	mov	WORD PTR ds:[0b5h*4+2], es

SkipRomInitialization:
	pop ax
	pop es
	pop ds
	popf
	retf

assume  cs:_TEXT
extrn _start: near
init proc near
	push  ax
	push  ax
	pushf
	push ax
	mov ax, 0
	jmp irq_entry
init	endp

public isr1
isr1 proc near
	push  ax
	push  ax
	pushf
	push ax
	mov ax, 1
	jmp irq_entry
isr1	endp

public _install13h
_install13h proc near
	pushf
	cli
	push ds
	mov ax, 0
	mov ds, ax
	les	ax, ds:[13h*4]
	mov WORD PTR ds:[013h*4], offset _TEXT:isr1
	mov WORD PTR ds:[013h*4+2], cs
	mov	WORD PTR ds:[034h*4], ax
	mov	WORD PTR ds:[034h*4+2], es
	pop ds
	popf
	ret
_install13h	endp

irq_entry proc near
	push cx
	push dx
	push bx
	push bp
	push si
	push di
	push ds
	push es

	mov bp,sp

	push ax

   	mov ax,DGROUP
	mov bx, cs
	add ax, bx
    assume  ds:DGROUP
	mov	ds, ax

	mov bx, 0
	mov es, bx

	pop si
	push si

	cmp si, 0
	jnz puti13
	les	bx, es:[0b5h*4]
	jmp get_done
puti13 label near
	les	bx, es:[034h*4]
get_done label near

	mov WORD PTR ss:[bp+20],bx
	mov WORD PTR ss:[bp+22],es

    assume  es:DGROUP
	mov es, ax

	sti
	cld

	pop bx
	push ss
	mov ax, bp
	push ax
	push bx

	call _start

	add sp, 6

	test ax,ax
	jz handled

	pop es
	pop ds
	pop di
	pop si
	pop bp
	pop bx
	pop dx
	pop cx
	pop ax
	popf
	retf
handled label near
	pop es
	pop ds
	pop di
	pop si
	pop bp
	pop bx
	pop dx
	pop cx
	pop ax
	popf
	add sp, 4
	iret

irq_entry	endp
_TEXT ends

	end
