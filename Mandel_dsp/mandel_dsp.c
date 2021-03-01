//**************************************************************************
//	Mandel_DSP by Wrangler, February 2021
//	Mandelbrot render program for the Amiga AA3000 or AA3000+ with DSP3210
//	Based on Mandel by Erik Trulsson 1996 and on ARTABROT by George T Warner
//	Use this code however you like but credit the authors as above
//**************************************************************************


#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/tasks.h>
#include <exec/execbase.h>

#include <clib/exec_protos.h> 
#include <clib/dos_protos.h>
#include <clib/timer_protos.h>

#include <hardware/cia.h>

int version = 00;


//**************************************************************************
//	DSP routine
//**************************************************************************

ULONG __attribute__((aligned(4))) dspcode[] = {   /* 32bit aligned! */
0x942f0278,	//9	r1 = AddressPR(Maxiter)	//address of Maxiter
0x946f0270,	//10	r3 = AddressPR(Maxiterad)
0x80000000,	//11	nop
0x9de11800,	//12	*r3 = r1	//fill in the address
			//13	
0x944f0064,	//14	r2 = AddressPR(Vec_tab)	//first long to copy
0xc001e000,	//15	r1 = 0x5003e000		//RAM0
0x90215003,	//15
0x80000000,	//16	2*nop
0x80000000,	//16
				//17	Copyloop:
0x9ce41017,	//18	r4 = *r2++
0x80000000,	//19	nop
0x9de40817,	//20	*r1++ = r4
0x98e01023,	//21	r2-r3
0x81afffe8,	//22	if(le) pcgoto Copyloop	//copy the prog to RAM0
0x80000000,	//23	nop
				//24	
0xc01ae000, //0x974f0038,	//25	r22 = 0x5003e000	//set our exception vector table to start of RAM0
0x935a5003, //0x80000000,	//25
0x946f0248,	//26	r3 = AddressPR(flag)    
0xc004ffee,	//27	r4 = 0xC0FFEE			//how else to get started?
0x908400c0,	//27
0x80000000,	//28	2*nop    
0x80000000,	//28
0x9de41800,	//29	*r3 = r4				//signal done
				//30	
				//31	poll:
0xc0048000,	//32	r4 = (ushort24) 0x8000
0x9d640408,	//33	emr = (short) r4		//enable Int 1
0x80000000,	//34	3*nop
0x80000000,	//34
0x80000000,	//34
0x9de0040a,	//35	waiti					//sit and wait for an interrupt
0x802fffe0,	//36	pcgoto poll
0x80000000,	//37	nop
				//38	
				//39	//**********************************************************************
				//40	// Exception vector table -- so we can redirect interrupts to our code
				//41	//
				//42	//**********************************************************************
				//43	
0x802f0078,	//44	Vec_tab:	if(true) pcgoto trap_unhandled	//reset
0x80000000,	//45				nop
0x802f0070,	//46				if(true) pcgoto trap_unhandled	//bus err
0x80000000,	//47				nop
0x802f0068,	//48				if(true) pcgoto trap_unhandled	//illegal instr
0x80000000,	//49				nop
0x802f0060,	//50				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//51				nop
0x802f0058,	//52				if(true) pcgoto trap_unhandled	//addr err
0x80000000,	//53				nop
0x802f0050,	//54				if(true) pcgoto trap_unhandled	//DSU over/underflow
0x80000000,	//55				nop
0x802f0048,	//56				if(true) pcgoto trap_unhandled	//NaN
0x80000000,	//57				nop
0x802f0040,	//58				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//59				nop
0x802f0038,	//60				if(true) pcgoto trap_unhandled	//Int 0
0x80000000,	//61				nop
0x802f0030,	//62				if(true) pcgoto trap_unhandled	//Timer
0x80000000,	//63				nop
0x802f0028,	//64				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//65				nop
0x802f0020,	//66				if(true) pcgoto trap_unhandled	//Boot ROM
0x80000000,	//67				nop
0x802f0018,	//68				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//69				nop
0x802f0010,	//70				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//71				nop
0x802f0008,	//72				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//73				nop
0x802f0024,	//74				if(true) pcgoto trap_int	//Int 1
0x9d600408,	//75				emr = (short) r0			//mask further ints
				//76	
				//77	
				//78	
				//79	//**********************************************************************
				//80	// Unhandled exceptions
				//81	//
				//82	//**********************************************************************
				//83	
				//84	trap_unhandled:
0x946f0190,	//85	r3 = AddressPR(flag)    
0xc004dead,	//86	r4 = 0xDEADDEAD    
0x9084dead,	//86
0x80000000,	//87	2*nop    
0x80000000,	//87
0x9de41800,	//88	*r3 = r4				//signal Error
0x9de0040a,	//89	waiti					//wait for an interrupt, except they're masked
0x802ffff8,	//90	pcgoto .
0x80000000,	//91	nop
				//92	
				//93	//**********************************************************************
				//94	// Interrupt-driven processing
				//95	//
				//96	//**********************************************************************
				//97	
				//98	trap_int:				//called when there is an int and this is where it all happens
0x94af0154,	//99	r5 = AddressPR(Maxiterad)
0x9ce52800,	//100	r5 = *r5				//get absolute address of Maxiter
0x80000000,	//101	nop
				//102	
0x94650014,	//103	r3 = r5 + (flag - Maxiter)    
0xc004feed,	//104	r4 = 0xD0FEED    
0x908400d0,	//104
0x80000000,	//105	2*nop    
0x80000000,	//105
0x9de41800,	//106	*r3 = r4				//signal started interrupt routine
				//107	
				//108	docalc:
0x980c0020,	//109	r12 = r0				//zero the pixel count
0x95450010,	//110	r10 = r5 + (Buffer - Maxiter)        // Set pointer to output data.
0x9cea5000,	//111	r10 = *r10
0x95c50000,	//112	r14 = r5 + (Maxiter - Maxiter)    // r14 = Maxiter    
0x9cee7000,	//113	r14 = *r14                    // Get the maximum number of iterations.  
0x950f0100,	//114	r8 = AddressPR(Scrfl)        // Set pointer to temp storage for floats         
				//115	
0x94450004,	//116	r2 = r5 + (Xstart - Maxiter)		// r2 = Xstart      
0x946f0104,	//117	r3 = AddressPR(myXstart) 
				//118	
				//119	// Use these to convert from IEEE to DSP3210 format.     
0x7e800b9f,	//120	*r3++ = a0 = dsp(*r2++)        //Xstart    
0x7e800b9f,	//121	*r3++ = a0 = dsp(*r2++)            //Ystart    
0x7e800818,	//122	*r3 = a0 = dsp(*r2)				//delta
				//123	
				//124	// r11 - iteration count.  
				//125	// r12 - pixel count     
				//126	// r14 - max iterations.    
				//127	// r8  - pointer to temp storage.     
				//128	Mloop:
				//129	// read in values
0x944f00f0,	//130	r2 = AddressPR(Scrlong)			//Temp storage for a long
0x9dec1000,	//131	*r2 = r12						//Store pixel count
0x946f00f4,	//132	r3 = AddressPR(myDelta)     
0x7c000807,	//133	a0 = float32(*r2)					//pixel count as a float
0x94cf00e8,	//134	r6 = AddressPR(myYstart)    // Set pointer to y
0x956f00d0,	//135	r11 = AddressPR(Zero)    
0x20661807,	//136	a3 = *r6 + a0 * *r3			//a3 = ystart + pixel x delta
0x946f00d8,	//137	r3 = AddressPR(myXstart)    // Set pointer to x    
0x30400c07,	//138	a2 = *r3					//a2 = xstart
0x30002c07,	//139	a0 = *r11                   // Zero the a0 accumulator.     
0x30202c07,	//140	a1 = *r11                    // Zero the a1 accumulator.     
0x30400147,	//141	*r8++ = a2 = a2                // Save real c.     
0x306001c6,	//142	*r8-- = a3 = a3                // Save imaginary c.     
0x980b0020,	//143	r11 = r0                    // Zero iteration count. 
				//144	
0x128f0078,	//145	Mandel:     pccall TestMag(r18)    
0x980b5837,	//146	r11 = r11 + 1                // Increment iteration count.     
				//147	
0x808f0028,	//148	if(ne) pcgoto Pointdone        // This point diverged; it's done.     
0x80000000,	//149	nop    
				//150	
0x98e0582e,	//151	r11 - r14                    // Compare to max. num. of iterations.     
0x818f001c,	//152	if(gt) pcgoto Pointdone        // Return if greater than or equal to.     
0x80000000,	//153	nop    
				//154	
				//155	// Now the famous z = z^2 + C function 
				//156	DoTheBrot:    
0x70404007,	//157	a2 = a0 * a1    
0x70600007,	//158	a3 = a0 * a0    
0x6c804087,	//159	a0 = a3 - a1 * a1            // This gives the real part of z squared.     
0x34208107,	//160	a1 = a2 + a2                // This gives the imaginary part of z squared.     
0x3411c007,	//161	a0 = a0 + *r8++                // This is the +C (real) part of the equation.     
0x802fffc8,	//162	pcgoto Mandel    
0x34318087,	//163	a1 = a1 + *r8--                // This is the +C (img.) part of the equation. 
				//164	
				//165	Pointdone:    
0x9d6b5017,	//166	*r10++ = (short)r11            // Save result.
0x944f008c,	//167	r2 = AddressPR(Points)		//Number of pixels per column
0x9ce21000,	//168	r2 = *r2
0x980c6037,	//169	r12 = r12 + 1
0x98e0102c,	//170	r2 - r12
0x818fff74,	//171	if(gt) pcgoto Mloop
0x80000000,	//172	nop
				//173	
0x94650014,	//174	r3 = r5 + (flag - Maxiter)    
0xc004feed,	//175	r4 = 0xD1DFEED    
0x90840d1d,	//175
0x80000000,	//176	2*nop    
0x80000000,	//176
0x9de41800,	//177	*r3 = r4				//signal finished interrupt routine    
0x80000000,	//178	3*nop
0x80000000,	//178
0x80000000,	//178
0x803e0000,	//179	ireturn					//return from interrupt and wait for next initiation
0x80000000,	//180	nop
				//181	
				//182	
				//183	//**********************************************************************
				//184	// TestMag -- subroutine to compare magnitude of complex number in a0    
				//185	//        and a1 to 2.0.  If greater, r1 = 1 upon exiting; if                
				//186	//        less, r1 = 0.                                                    
				//187	//**********************************************************************
				//188	TestMag:    
0x70600007,	//189	a3 = a0 * a0                // Square real part.     
0x6c604087,	//190	a3 = a3 + a1 * a1            // Add to img. part squared.     
0x944f0020,	//191	r2 = AddressPR(Four)    
0x34e40187,	//192	a3 = a3 - *r2                // Compare to 4.0.     
0x80000000,	//193	nop    
0x80000000,	//194	nop    
0x80000000,	//195	nop    
0x826f0004,	//196	if(alt) pcgoto Donef        // If negative result, goto Done.     
0x98010020,	//197	r1 = r0    
0x14200001,	//198	r1 = (short)1                // Result did diverge. 
				//199	Donef:    
0x80340000,	//200	return(r18)    
0x9ae10000,	//201	r1 - 0                        // Set flags based on result. 
				//202	
				//203	
				//204	
				//205	
				//206	//**********************************************************************
				//207	// Processing, input and output data, and flags
				//208	//
				//209	//**********************************************************************
				//210	
0x00000084,	//211	Four:        float 16.0            // Number for magnitude comparisons. 
0x00000000,	//212	Zero:        float 0.0
0x00000000,	//213	Scrfl:        float 0.0            // Scratch pad. 
0x00000000,	//214				float 0.0
0x00000000,	//215	Scrlong:	long 0
				//216	
0x00000000,	//217	myXstart:    float    0.0
0x00000000,	//218	myYstart:    float    0.0
0x00000000,	//219	myDelta:	float	0.0
0x00000100,	//220	Points:		long 256
0x00000000,	//221	Maxiterad:	long 0
				//222	
				//223	// Note: Xstart and Ystart all hold IEEE representations of    
				//224	// floating point values.  Hence, they have been declared as longs    
				//225	// in order that they not be confused with DSP floats.                
				//226	
0x0000003f,	//227	Maxiter:    long    63        // Maximum number of iterations. 
0x00000050,	//228	Xstart:        long    80        // Starting real (x) coordinate. 
0x00000050,	//229	Ystart:        long    80        // y coordinate.
0x00000001,	//230	Delta:		long 1			//addition to y coord for each pixel 
0x00000000,	//231	Buffer:		long	0		//which buffer to use
0x00000000,	//232	flag:        long    0
				//233	OutputData:     // Output either maxiter or not 

			//207	OutputData:     // Output either maxiter or not 
			//8 * 16 * long = 256 shorts = 1 column of pixels
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//buffer 1
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	//buffer 2
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


#define Maxiter 0x280
#define XStart 0x284
#define YStart 0x288
#define delta 0x28c
#define OutputData 0x290 //=Buffer
#define flag 0x294
#define OutputData1 0x298
#define OutputData2 0x498
#define outputlen 8*16	//measured in longs
#define paramlen 5

//**************************************************************************
//	Globals
//**************************************************************************

typedef struct out_params
{
	volatile long Flag;
	volatile short Outputdata[outputlen*2*2-1];	//outputlen is longs not shorts, and there are 2
} out_params;

typedef struct in_params
{
	volatile long MaxIter;
	volatile float Xstart;
	volatile float Ystart;
	volatile float Delta;
	volatile long *Outputdata;
} in_params;


extern struct CIA __far ciaa;
/* DSP control registers. */

ULONG * volatile dsp_read = (void *) 0x00dd005C;
UBYTE * volatile dsp_write = (void *) 0x00dd0080;
ULONG * volatile zero = NULL;

volatile struct out_params *output_v = (struct out_params *) &dspcode[flag>>2];
struct in_params *input_v = (struct in_params *) &dspcode[Maxiter>>2];;

int processor = 2;
ULONG cache_orig, cache_curr, cache_recall;
#define cache_mask 0x80000100		//copyback and data cache enable


#define width 320
#define height 256
#define depth 5


#define double float

struct Device *TimerBase;
static struct IORequest timereq;

struct IntuitionBase * IntuitionBase=0;
struct GfxBase * GfxBase=0;
struct Screen * scrn=0;
struct Window * win=0;
struct RastPort * rp;
struct MsgPort * mp;
struct IntuiMessage * msg;
void Cleanup(void);
void HandleMsg(void);
ULONG secs,msecs,olds=0,oldms=0;
void zoomin(WORD,WORD);
void zoomout(WORD,WORD);
void drawmandel(void);
double x1,x2,y11,y2;
int maxcount;
jmp_buf jb;
int oldpri=0;

struct DrawInfo  *drawinfo;

UBYTE ct[][3]=
	{
	{ 11, 2, 2 },
	{ 11, 11, 11},
	{ 0, 10, 2 },
	{ 8, 8, 8 },
	{ 2, 2, 12 },
	{ 13, 13, 13 },
	{ 11, 0, 12 },
	{ 11, 11, 1 },
	{ 2, 10, 12 },
	{ 1, 5, 14 },
	{ 7, 8, 10 },
	{ 14, 14, 13 }
	};



//**************************************************************************
//	Support routines for the DSP
//**************************************************************************

void SetCtrl(ULONG val) {
	short i;
	ULONG mask = 0x4c;

	do {
		*dsp_write = (UBYTE)(val & 0xff);

		for (i = 0; i<256; ++i);
		if ((*dsp_read & mask) == (val & mask)) break;
		printf("**Control Write Failure: $%2lx != $%2lx\n",*dsp_read & mask,val & mask);
	} while (TRUE);
}

#define BASICWAIT 100000
#define RETRYTHRESHOLD 10

BOOL WakeupWait(void) {
	ULONG count;
	short tcnt = 0;

	do {
		count = 0;
		do {
			if (*zero != (ULONG)dspcode) break;
			++count;
		} while (count < BASICWAIT);
		if (*zero != (ULONG)dspcode) return TRUE;

		printf(" -- wakeup timeout\n");
		if (++tcnt > RETRYTHRESHOLD) return FALSE;
	} while (TRUE);
}

void InitDSP(void) {
	long i, j;

	SetCtrl(0x7fL); /* Set up for DSP in reset */

	for (i = 0; i < 1000; i++) /* Give it a small but reliable pulse */
		j = ciaa.ciapra;

	SetCtrl(0xff); /* Take DSP out of reset */
	SetCtrl(0xfd); /* cause int1 on dsp */
	if (!WakeupWait()) { /* Wait for DSP to wake up */
		printf(" ** DSP failed to wakeup\n");
		exit(EXIT_FAILURE);
	}
	
	if(output_v->Flag != 0xC0FFEE) {
		printf(" ** DSP failed to initiliase and return magic cookie: 0x%08x\n",output_v->Flag);
		//exit(EXIT_FAILURE);
	}
}

void DSP_int() {
	output_v->Flag = 0x0;
	SetCtrl(0xfd); /* cause int1 on dsp */
}

void DSP_waitready() {
	int i;
	long j;
	volatile long v;
	ULONG cachelen = 4;
	CachePostDMA(&dspcode[flag>>2],&cachelen,0); //clear read cache for flag

	if(output_v->Flag ==  0xC0FFEE) return; //got initialised signal
	for(i=0; i<BASICWAIT/10; i++) {
		cachelen = 4;
		CachePostDMA(&dspcode[flag>>2],&cachelen,0); //clear read cache for flag
		if(output_v->Flag ==  0xD1DFEED) {
			cachelen = 4*(outputlen*2);
			CachePostDMA(&dspcode[OutputData1>>2],&cachelen,0);	//clear entire data area
			return; //got ready signal
		}
		j = ciaa.ciapra;
	}
	printf(" ** DSP failed to return ready cookie in a reasonable amount of time: 0x%08x\n",output_v->Flag);
	//could also check for 0xD0FEED
}


void dumpmem()
{
	int i;

	printf("DSP memory dump\n");
	for(i=Maxiter>>2; i<(sizeof(dspcode)>>2); i++)
	{
		printf("Loc: 0x%08x, value: 0x%08x\n",i<<2, (ULONG) *(dspcode+i));
	}
	printf("\n");
}


//**************************************************************************
//	Main entry point
//**************************************************************************

int main(int argc,char **argv)
	{
	int i;
	struct IntuiText  myIText;
	struct TextAttr   myTextAttr;
	char *timetext;
	char buffer[30];
	ULONG myTEXTPEN;
	ULONG myBACKGROUNDPEN;

	struct timeval start, end;
	ULONG timetaken;

	if(argc!=2 && argc!=1)
		{
		printf("Usage mandel_dsp\n");
		exit(EXIT_FAILURE);
		}
		
	/*if(argc==6)	
		{
		x1=atof(argv[1]);
		x2=atof(argv[2]);	
		y11=atof(argv[3]);
		y2=atof(argv[4]);
		maxcount=atoi(argv[5]);
		}*/

	else	/* no arguments given */
		{
		x1=-2.2;
		x2=1.1;
		y11=-1.4;
		y2=1.4;
		maxcount=50;
		}
		
	printf("Original code by Erik Trulsson 1996.\n");
	printf("DSP coding Wrangler Feb 2021 based on ARTABROT by George T Warner\n");
	printf("Version 3.%d\n", version);
	printf("Usage: double click to zoom, esc or q quits, Z zoom out\n");
	printf("d use DSP, f use FPU\n");

	OpenDevice("timer.device", 0, &timereq, 0);
	TimerBase = timereq.io_Device;
			
	if(atexit(Cleanup)) return EXIT_FAILURE;
	if(!(IntuitionBase=(struct IntuitionBase *) OpenLibrary("intuition.library",37))) exit(EXIT_FAILURE);
	if(!(GfxBase=(struct GfxBase *) OpenLibrary("graphics.library",37))) exit(EXIT_FAILURE);
	
	scrn=OpenScreenTags(NULL,
						SA_Width,width,
						SA_Height,height,
						SA_Depth,depth,
						SA_ShowTitle,FALSE,
						SA_Type,CUSTOMSCREEN,
			/*			SA_DisplayID,HIRES_KEY, */
						TAG_DONE);
	if(!scrn) exit(EXIT_FAILURE);
	win=OpenWindowTags(NULL,
						WA_Left,0,
						WA_Top,0,
						WA_Width,width,
						WA_Height,height,
						WA_CustomScreen,(ULONG) scrn,
						WA_Borderless,TRUE,
						WA_Backdrop,TRUE,
						WA_Activate,TRUE,
						WA_SmartRefresh,TRUE,
						WA_NoCareRefresh,TRUE,
						WA_RMBTrap,TRUE,
						WA_IDCMP,IDCMP_VANILLAKEY|IDCMP_MOUSEBUTTONS,
						TAG_DONE);
	if(!win) exit(EXIT_FAILURE);
	drawinfo = GetScreenDrawInfo(scrn);
	if(!drawinfo) {
		printf("Failed to get drawinfo!\n");
		exit(EXIT_FAILURE);
	}
	myTEXTPEN = drawinfo->dri_Pens[TEXTPEN];
	myBACKGROUNDPEN  = drawinfo->dri_Pens[BACKGROUNDPEN];

	/* create a TextAttr that matches the specified font. */
	myTextAttr.ta_Name  = drawinfo->dri_Font->tf_Message.mn_Node.ln_Name;
	myTextAttr.ta_YSize = drawinfo->dri_Font->tf_YSize;
	myTextAttr.ta_Style = drawinfo->dri_Font->tf_Style;
	myTextAttr.ta_Flags = drawinfo->dri_Font->tf_Flags;
	myIText.FrontPen    = myTEXTPEN;
	myIText.BackPen     = myBACKGROUNDPEN;
	myIText.DrawMode    = JAM2;
	myIText.LeftEdge    = 0;
	myIText.TopEdge     = 0;
	myIText.ITextFont   = &myTextAttr;
	myIText.NextText    = NULL;

	*zero = (LONG) dspcode; /* dsp reads PC addr from here after int1 */
	InitDSP(); /* Reset and set up DSP */

	input_v->MaxIter = maxcount+1;

	rp=win->RPort;
	mp=win->UserPort;
	oldpri=SetTaskPri(FindTask(0),-1);
	
	for(i=0;i<=11;i++)
		{
		SetRGB4(&(scrn->ViewPort),i+20,ct[i][0],ct[i][1],ct[i][2]);
		}
	
	setjmp(jb);
	SetRast(rp,0);
	GetSysTime(&start);
	drawmandel();

	GetSysTime(&end);
	SubTime(&end, &start);
	timetaken = end.tv_secs;// +*/ (end.tv_micro/1000000.f);

	switch(processor)
	{
	case 1:
		snprintf(buffer, 30, "DSP took %d seconds", timetaken);
		break;

	case 2:
		snprintf(buffer, 30, "FPU took %d seconds", timetaken);
		break;
	}

	myIText.IText = buffer;
	
	PrintIText(win->RPort,&myIText,10,10);
	
	while(1)
		{
		WaitPort(mp);
		HandleMsg();
		}
	
	SetCtrl(0xff);
	
	return 0;
	}
	
	
	
	
void Cleanup(void)	
	{
	//cache_curr = CacheControl(cache_orig, cache_mask);
	if(win!=NULL) CloseWindow(win);
	if(drawinfo!=NULL) FreeScreenDrawInfo(scrn,drawinfo);
	if(scrn!=NULL) CloseScreen(scrn);
	if(GfxBase!=NULL) CloseLibrary((struct Library *)GfxBase);
	if(IntuitionBase!=NULL) CloseLibrary((struct Library *)IntuitionBase);
	CloseDevice(&timereq);
	SetTaskPri(FindTask(NULL),oldpri);
	}

//**************************************************************************
//	Intuition message handler
//**************************************************************************
	
void HandleMsg(void)
	{
	if(msg=(struct IntuiMessage *)GetMsg(mp))
		{
		switch(msg->Class)
			{
			case IDCMP_MOUSEBUTTONS:
				if((msg->Code) == SELECTDOWN)			
					{
					secs=msg->Seconds;	
					msecs=msg->Micros;
					if(DoubleClick(olds,oldms,secs,msecs))
						{
						olds=secs;	
						oldms=msecs;
						zoomin(msg->MouseX,msg->MouseY);
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						}
					else
						{
						olds=secs;
						oldms=msecs;
						}
					}
				else if((msg->Code) == MENUDOWN )
					{
					secs=msg->Seconds;	
					msecs=msg->Micros;
					if(DoubleClick(olds,oldms,secs,msecs))
						{
						olds=secs;	
						oldms=msecs;
						zoomout(msg->MouseX,msg->MouseY);
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						}
					else
						{
						olds=secs;
						oldms=msecs;
						}
					}
				break;
			case IDCMP_VANILLAKEY:
				switch(msg->Code)
					{	
					case 'q':
					case 27:  /* Escape key */
						ReplyMsg((struct Message *)msg);
						Cleanup();
						*zero = 0;
						SetCtrl(0xff);
						exit(0);
					case 'z':
						zoomin(width/2,height/2);
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						exit(EXIT_FAILURE); /* should never reach here */
					case 'Z':
						zoomout(width/2,height/2);
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						exit(EXIT_FAILURE); /* should never reach here */
					case 'd':
					case 'D':
						processor = 1;
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						//exit(EXIT_FAILURE); /* should never reach here */
					case 'f':
					case 'F':
						processor = 2;
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						//exit(EXIT_FAILURE); /* should never reach here */
					}
			}				
		ReplyMsg((struct Message *)msg);
		}
	}


//**************************************************************************
//	Render routines 
//**************************************************************************

void zoomin(WORD x,WORD y)	
	{
	double nx,ny;
	double dx,dy;
	maxcount+=25;
	dx=(x2-x1);	
	dy=(y2-y11);
	
	nx=(dx*x)/width+x1;
	ny=(dy*(height-y))/height+y11;
	x1=nx-(dx/2)/2;
	x2=nx+(dx/2)/2;
	y11=ny-(dy/2)/2;
	y2=ny+(dy/2)/2;
	}
	
	
void zoomout(WORD x,WORD y)
	{
	double nx,ny;
	double dx,dy;
	maxcount-=25;
	if(maxcount<10) maxcount=10;
	dx=(x2-x1);	
	dy=(y2-y11);
	
	nx=(dx*x)/width+x1;
	ny=(dy*(height-y))/height+y11;
	x1=nx-(dx/2)*2;
	x2=nx+(dx/2)*2;
	y11=ny-(dy/2)*2;
	y2=ny+(dy/2)*2;	
	}

void drawmandel(void)
	{
	int i,j,j1;
	double a,b,am,bm;
	double u,v,w,x,y;

	int k;
	ULONG cachelen;
	ULONG *outptr;

	am=(x2-x1)/width;
	bm=(y11-y2)/height;
	
	for(i=0,a=x1;i<width;i++)	
		{
		switch(processor)
			{
			case 1:
				b=y2;
				outptr = ((i & 1) == 0 )? &dspcode[OutputData1>>2] : &dspcode[OutputData2>>2];	//double buffer 
				DSP_waitready();	//don't set up the DSP until it has finished processing
				input_v->Xstart = a;
				input_v->Ystart = b;
				input_v->Delta = bm;
				input_v->Outputdata = outptr;

				cachelen = 4*(paramlen);
				CachePreDMA(&dspcode[Maxiter>>2],&cachelen,0);
				DSP_int();	//DSP processes a column

				if(i) {	//skip for first iteration
					for(j=0;j<height;)		//colour pixels
						{
						HandleMsg();
						for(j1=0;j1<8 && j<height;j1++,j++)
							{
							if(output_v->Outputdata[j+(((i & 1) == 0)? 256 : 0)]-2>=maxcount)
								{
								SetAPen(rp,1);
								}
							else
								{
								SetAPen(rp,(output_v->Outputdata[j+(((i & 1) == 0)? 256 : 0)]-2) % (1<<depth));
								}
							WritePixel(rp,i-1,j);	//colour previous column
							}
						}
				}
				break;
					
			case 2:
				for(j=0,b=y2;j<height;)
					{
					HandleMsg();
					for(j1=0;j1<8 && j<height;j1++,j++)
						{
						x=a;y=b;
							
						for(k=0;k<maxcount;k++)
							{
							u=x*x;
							v=y*y;
							if(u+v>16) break;
							w=2*x*y;
							x=u-v+a;
							y=w+b;
							}
						
						if(k>=maxcount)
							{
							SetAPen(rp,1);
							}
						else
							{
							SetAPen(rp,k % (1<<depth));
							}
						WritePixel(rp,i,j);
						b+=bm;
						}
					}
				break;
			}	//switch

		a+=am;
		}
	if(processor==1) {
		//draw in final column
		DSP_waitready();	//wait for final column to be ready
		for(j=0;j<height;)		//colour pixels
		{
			HandleMsg();
			for(j1=0;j1<8 && j<height;j1++,j++)
			{
				if(output_v->Outputdata[j+(((width & 1) == 0)? 256 : 0)]-2>=maxcount)
					{
					SetAPen(rp,1);
					}
				else
					{
					SetAPen(rp,(output_v->Outputdata[j+(((width & 1) == 0)? 256 : 0)]-2) % (1<<depth));
					}
				WritePixel(rp,width-1,j);	//colour final column
			}
		}
	}
}
