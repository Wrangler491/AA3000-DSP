
*** EXPERIMENTAL - THERE WILL BE BUGS!!***

This is an implementation of an assembler for the DSP3210.  The host machine needs 
to be x86 running linux (I use cygwin64, so that definitely works but other linux-type OSes 
ought to work).  

Created building on the work of Tom Roberts' ASM32 for the DSP32C, with many amendments 
by Wrangler 2021.

Usage:

./asm3210 foo.s

will provide:
foo.dsp - a binary dump of the output
foo.lst - a listing of the assembly

Notes:
- This assembler generally matches the output from d32as but there are some exceptions:
	- an instruction like r1 = 1 can be encoded in more than one way
	  asm3210 uses "set", so a straight 16 bit load to r1 whereas
	  d32as assembles it as r1 = r0 + 1 ie "add" (remembering r0 is always 0)
	- d32as assembles r1 = 0xC0FFEE as a 32 bit load over two instructions
	  asm3210 assembles this instruction using 24 unsigned values saving 4 bytes(!)
- No symbol table is created (yet)
- The assembler skips directives such as .global and @P, so no syntax error but no code is generated either
- Updates to the files might happen at any time.

For bug reports, find me on a1k.org

Wrangler 19/7/2021

Version log:

19/7/2021
Defines now supported - asm3210 will assemble the source for mandel_dsp
Added the instructions rD = rS * 2 and rD = rS / 2, which just map to add and arithmetic
shift right instructions

25/6/2021
First version