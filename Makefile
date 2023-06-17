

.PHONY: clean all default

default: optionrom.bin

all: default

%.obj: %.c inlines.h biosint.h
	${WATCOM}/binl64/wcc -0 -wx -s -oatxh -hd -d0 -ms -zu -za99 -we  -fo$@ $<

%.obj: %.S 
	${WATCOM}/binl64/wasm -0 -wx -ms -fo$@ $<

%.lss: %.obj
	${WATCOM}/binl64/wdis $< > $@

optionrom.bin: main.obj init.obj print.obj link.lnk
	${WATCOM}/binl64/wlink name $@ @link.lnk
	./checksum.py $@

bios.bin: optionrom.bin bios.bin.orig
	dd if=bios.bin.orig of=bios.bin
	dd if=optionrom.bin of=bios.bin conv=notrunc bs=1k seek=8