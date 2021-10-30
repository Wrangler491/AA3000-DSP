
//*************************************************************************
// DSP3210_init.s
// DSP3210 assembly language code for system support
// By Wrangler October 2021
// Use this code however you like but you must credit me 
//*************************************************************************

.global _dspcode
.global _DSP_flag
.extern	_Maxiter
.extern DSP_shareddata_address
.extern DSP_codestart
.extern DSP_codeend
.extern DSP_shareddata

/* Defines for DSP_flag */
#define DSP3210_OK		0x00C0FFEE
#define DSP3210_RUN		0x00D0FEED
#define DSP3210_READY	0x0D1DFEED
#define DSP3210_FAIL	0xDEADDEAD

/* Defines for DSP onboard RAM */
#define DSP3210_RAM0	0x5003F000
#define DSP3210_RAM1	0x5003E000

// Macro for PC relative addressing. 
#define AddressPR(LAB) pc + LAB - (.+8)    

//**********************************************************************
// Initialisation to set up vector table and wait for int
//
//**********************************************************************
_dspcode:
r1 = DSP_shareddata	//address of start of shared data area that M68K can also access
r3 = DSP_shareddata_address
nop
*r3 = r1	//fill in the address

r2 = Vec_tab	//first long to copy
r3 = Cache_end
r1 = DSP3210_RAM1		//RAM1
Copyloop1:
r4 = *r2++
nop
*r1++ = r4
r2-r3
if(lt) pcgoto Copyloop1	//copy the prog to RAM1
nop

r2 = DSP_codestart	//first long to copy
r3 = DSP_codeend

Copyloop2:
r4 = *r2++
nop
*r1++ = r4
r2-r3
if(le) pcgoto Copyloop2	//copy the prog to RAM0
nop

r3 = _DSP_flag    
r4 = DSP3210_OK			//how else to get started?
r22 = DSP3210_RAM1 	//set our exception vector table pointer   
*r3 = r4				//signal done

poll:
r4 = (ushort24) 0x8000
emr = (short) r4		//enable Int 1
waiti					//sit and wait for an interrupt
nop
r3 = _DSP_flag
r4 = DSP3210_READY
nop    
*r3 = r4				//signal finished interrupt routine  
pcgoto poll
nop


//**********************************************************************
// DSP flag for signalling M68k
//
//**********************************************************************

_DSP_flag:		long    0



//**********************************************************************
// Exception vector table -- so we can redirect interrupts to our code
//
//**********************************************************************

Vec_tab:	if(true) pcgoto trap_unhandled	//reset
			nop
			if(true) pcgoto trap_unhandled	//bus err
			nop
			if(true) pcgoto trap_unhandled	//illegal instr
			nop
			if(true) pcgoto trap_unhandled	//reserved
			nop
			if(true) pcgoto trap_unhandled	//addr err
			nop
			if(true) pcgoto trap_unhandled	//DSU over/underflow
			nop
			if(true) pcgoto trap_unhandled	//NaN
			nop
			if(true) pcgoto trap_unhandled	//reserved
			nop
			if(true) pcgoto trap_unhandled	//Int 0
			nop
			if(true) pcgoto trap_unhandled	//Timer
			nop
			if(true) pcgoto trap_unhandled	//reserved
			nop
			if(true) pcgoto trap_unhandled	//Boot ROM
			nop
			if(true) pcgoto trap_unhandled	//reserved
			nop
			if(true) pcgoto trap_unhandled	//reserved
			nop
			if(true) pcgoto trap_unhandled	//reserved
			nop
			if(true) pcgoto trap_int	//Int 1
			emr = (short) r0			//mask further ints

//**********************************************************************
// Unhandled exceptions
//
//**********************************************************************

trap_unhandled:
r3 = _DSP_flag    
r4 = DSP3210_FAIL    
2*nop    
*r3 = r4				//signal Error
waiti					//wait for an interrupt, except they're masked
pcgoto .
nop

//**********************************************************************
// Interrupt-driven processing
//
//**********************************************************************

trap_int:				//called when there is an int and this is where it all happens
r3 = _DSP_flag     
r4 = DSP3210_RUN    
nop    
*r3 = r4				//signal started interrupt routine

Cache_end: