
*** EXPERIMENTAL - THERE WILL BE BUGS!!***

This is an implementation of a gcc 2.95.3 cross-compiler for the DSP3210.  It is a compiler only, no assembler.  The host machine needs to be x86 running linux (I use cygwin64, so that definitely works but other linux-type OSes ought to work).  

Created building on the work of Michael Collinson, with many amendments by Wrangler 2021, and some parts are copyright Free Software Foundation Inc.

Usage:

./xgcc -B./ -S foo.c -o foo.s

Notes:
- Do not try to use optimisation options (eg -O2) - it's not implemented yet and you will likely get a fatal internal compiler error - TO DO
- Compiler doesn't know do loops yet - TO DO
- When pushing floats to the stack, the compiler will make use of a scratch register (eg r1) and execute sp = r1.  Some assemblers complain at this instruction, but it's valid according to the DSP3210 Information Manual.
- Updates to the files might happen at any time.

Wrangler 13/6/2021