#	makefile for mandel_dsp.exe

all:	mandel_dsp.exe 

clean:
	rm  mandel_m68k.o DSP3210.o m68kfinal.o mandelfinal.o mandel.o DSP3210_init.o mandel_dsp.exe

mandel_m68k.o: mandel_m68k.c
	vc +aos68k -cpu=68020 -fpu=68881 -I$(VBCC)/targets/m68k-amigaos/include -I$(NDK_INC) -I. -c -o mandel_m68k.o mandel_m68k.c

DSP3210.o: DSP3210.c
	vc +aos68k -cpu=68020 -fpu=68881 -I$(VBCC)/targets/m68k-amigaos/include -I$(NDK_INC) -I. -c -o DSP3210.o DSP3210.c

m68kfinal.o:	mandel_m68k.o DSP3210.o
	vlink -b elf32ppcbe -r -o m68kfinal.o  $(VBCC_LIB)/startup.o mandel_m68k.o DSP3210.o -L$(VBCC_LIB) -lvc -lamiga -lm881

mandelfinal.o: m68kfinal.o mandel.o DSP3210_init.o
	vlink -r -b elf32ppcbe -o mandelfinal.o m68kfinal.o mandel.o DSP3210_init.o

mandel.o: mandel.s
	../asm32-master/asm3210 -o mandel.o mandel.s

DSP3210_init.o: DSP3210_init.s
	../asm32-master/asm3210 -o DSP3210_init.o DSP3210_init.s

mandel_dsp.exe: mandelfinal.o
	../elf2exem68k/elf2exem68k.exe -o mandel_dsp.exe mandelfinal.o
	