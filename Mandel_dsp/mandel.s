
//*************************************************************************
// Mandel.s
// DSP3210 assembly language code to calculate Mandelbrots
// By Wrangler October 2021
// Based on Mandel by Erik Trulsson 1996 and on ARTABROT by George T Warner
// Use this code however you like but you must credit me and the two other 
// authors as above
//*************************************************************************

//export labels
//globals for DSP set up code
.global DSP_codestart
.global DSP_codeend
.global DSP_shareddata
.global DSP_shareddata_address

//globals for M68k C code
.global _Maxiter
.global _XStart
.global _YStart
.global _Delta
.global _Buffer
.global _OutputData1
.global _OutputData2
.extern _DSP_flag

// Macro for PC relative addressing. 
#define AddressPR(LAB) pc + LAB - (.+8)    

DSP_codestart:
r5 = AddressPR(DSP_shareddata_address)
r5 = *r5				//get absolute address of _Maxiter

docalc:
r12 = r0				//zero the pixel count
r10 = r5 + (_Buffer - _Maxiter)        // Set pointer to output data.
r10 = *r10
r14 = r5 + (_Maxiter - _Maxiter)    // r14 = _Maxiter    
r14 = *r14                    // Get the maximum number of iterations.  
r8 = AddressPR(Scrfl)        // Set pointer to temp storage for floats         

r2 = r5 + (_XStart - _Maxiter)		// r2 = _XStart      
r3 = AddressPR(myXstart) 

// Use these to convert from IEEE to DSP3210 format.     
*r3++ = a0 = dsp(*r2++)        //_XStart    
*r3++ = a0 = dsp(*r2++)            //_YStart    
*r3 = a0 = dsp(*r2)				//delta

// r11 - iteration count.  
// r12 - pixel count     
// r14 - max iterations.    
// r8  - pointer to temp storage.     
Mloop:
// read in values
r2 = AddressPR(Scrlong)			//Temp storage for a long
*r2 = r12						//Store pixel count
r3 = AddressPR(myDelta)     
a0 = float32(*r2)					//pixel count as a float
r6 = AddressPR(myYstart)    // Set pointer to y
r11 = AddressPR(Zero)    
a3 = *r6 + a0 * *r3			//a3 = ystart + pixel x delta
r3 = AddressPR(myXstart)    // Set pointer to x    
a2 = *r3					//a2 = xstart
a0 = *r11                   // Zero the a0 accumulator.     
a1 = *r11                    // Zero the a1 accumulator.     
*r8++ = a2 = a2                // Save real c.     
*r8-- = a3 = a3                // Save imaginary c.     
r11 = r0                    // Zero iteration count. 

Mandel:     pccall TestMag(r18)    
r11 = r11 + 1                // Increment iteration count.     

if(ne) pcgoto Pointdone        // This point diverged; it's done.     
nop    

r11 - r14                    // Compare to max. num. of iterations.     
if(gt) pcgoto Pointdone        // Return if greater than or equal to.     
nop    

// Now the famous z = z^2 + C function 
DoTheBrot:    
a2 = a0 * a1    
a3 = a0 * a0    
a0 = a3 - a1 * a1            // This gives the real part of z squared.     
a1 = a2 + a2                // This gives the imaginary part of z squared.     
a0 = a0 + *r8++                // This is the +C (real) part of the equation.     
pcgoto Mandel    
a1 = a1 + *r8--                // This is the +C (img.) part of the equation. 

Pointdone:    
*r10++ = (short)r11            // Save result.
r2 = AddressPR(Points)		//Number of pixels per column
r2 = *r2
r12 = r12 + 1
r2 - r12
if(gt) pcgoto Mloop
nop
 
ireturn					//return from interrupt and wait for next initiation
nop


//**********************************************************************
// TestMag -- subroutine to compare magnitude of complex number in a0    
//        and a1 to 2.0.  If greater, r1 = 1 upon exiting; if                
//        less, r1 = 0.                                                    
//**********************************************************************
TestMag:    
a3 = a0 * a0                // Square real part.     
a3 = a3 + a1 * a1            // Add to img. part squared.     
r2 = AddressPR(Four)    
a3 = a3 - *r2                // Compare to 4.0.     
nop    
nop    
nop    
if(alt) pcgoto Donef        // If negative result, goto Done.     
r1 = r0    
r1 = (short)1                // Result did diverge. 
Donef:    
return(r18)    
r1 - 0                        // Set flags based on result. 

//**********************************************************************
// Processing, input and output data, and flags
//
//**********************************************************************

Four:		float 16.0            // Number for magnitude comparisons. 
Zero:		float 0.0
Scrfl:		float 0.0            // Scratch pad. 
			float 0.0
Scrlong:	long 0

myXstart:	float    0.0
myYstart:	float    0.0
myDelta:	float	0.0
Points:		long 256
DSP_shareddata_address:	long 0
DSP_codeend:

// Note: _XStart and _YStart all hold IEEE representations of    
// floating point values.  Hence, they have been declared as longs    
// in order that they not be confused with DSP floats.                

DSP_shareddata:
_Maxiter:	long    63		// Maximum number of iterations. 
_XStart:	long    80		// Starting real (x) coordinate. 
_YStart:	long    80		// y coordinate.
_Delta:		long	1		//addition to y coord for each pixel 
_Buffer:	long	0		//which buffer to use
_OutputData1:     // Output either _Maxiter or not 
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	//buffer1
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
_OutputData2:
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	//buffer2
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
			short 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

