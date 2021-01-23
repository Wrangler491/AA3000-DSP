//**************************************************************************
//	Mandel_DSP by Wrangler, January 2021
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


//**************************************************************************
//	DSP routine
//**************************************************************************

ULONG __attribute__((aligned(4))) dspcode[] = {   /* 32bit aligned! */
			//1	// Macro for PC relative addressing. 
			//2	#define AddressPR(LAB) pc + LAB - (.+8)    
			//3	
			//4	//**********************************************************************
			//5	// Initialisation to set up vector table and wait for int
			//6	//
			//7	//**********************************************************************
			//8	
0x974f0168,	//9	r22 = AddressPR(Vec_tab)	//set our exception vector table
0x946f0218,	//10	r3 = AddressPR(flag)    
0xc004ffee,	//11	r4 = 0xC0FFEE			//how else to get started?
0x908400c0,	//11
0x80000000,	//12	2*nop    
0x80000000,	//12
0x9de41800,	//13	*r3 = r4				//signal done
			//14	
			//15	poll:
0xc0048000,	//16	r4 = (ushort24) 0x8000
0x9d640408,	//17	emr = (short) r4		//enable Int 1
0x80000000,	//18	3*nop
0x80000000,	//18
0x80000000,	//18
0x9de0040a,	//19	waiti					//sit and wait for an interrupt
0x802fffe0,	//20	pcgoto poll
0x80000000,	//21	nop
			//22	
			//23	//**********************************************************************
			//24	// Unhandled exceptions
			//25	//
			//26	//**********************************************************************
			//27	
			//28	trap_unhandled:
0x946f01e0,	//29	r3 = AddressPR(flag)    
0xc004dead,	//30	r4 = 0xDEADDEAD    
0x9084dead,	//30
0x80000000,	//31	2*nop    
0x80000000,	//31
0x9de41800,	//32	*r3 = r4				//signal Error
0x9de0040a,	//33	waiti					//wait for an interrupt, except they're masked
0x802ffff8,	//34	pcgoto .
0x80000000,	//35	nop
			//36	
			//37	//**********************************************************************
			//38	// Interrupt-driven processing
			//39	//
			//40	//**********************************************************************
			//41	
			//42	trap_int:				//called when there is an int and this is where it all happens
			//43	//r3 = AddressPR(flag)    
			//44	//r4 = 0xD0FEED    
			//45	//2*nop    
			//46	//*r3 = r4				//signal started interrupt routine
			//47	
			//48	docalc:
0x980c0020,	//49	r12 = r0				//zero the pixel count
0x954f01bc,	//50	r10 = AddressPR(OutputData)        // Set pointer to output data.       
0x95cf01a4,	//51	r14 = AddressPR(Maxiter)    // r14 = Maxiter    
0x9cee7000,	//52	r14 = *r14                    // Get the maximum number of iterations.  
0x950f0180,	//53	r8 = AddressPR(Scrfl)        // Set pointer to temp storage for floats         
			//54	
0x944f019c,	//55	r2 = AddressPR(Xstart)		// r2 = Xstart      
0x946f0184,	//56	r3 = AddressPR(myXstart) 
			//57	
			//58	// Use these to convert from IEEE to DSP3210 format.     
0x7e800b9f,	//59	*r3++ = a0 = dsp(*r2++)        //Xstart    
0x7e800b9f,	//60	*r3++ = a0 = dsp(*r2++)            //Ystart    
0x7e800818,	//61	*r3 = a0 = dsp(*r2)				//delta
			//62	
			//63	// r11 - iteration count.  
			//64	// r12 - pixel count     
			//65	// r14 - max iterations.    
			//66	// r8  - pointer to temp storage.     
			//67	Mloop:
			//68	// read in values
0x944f0170,	//69	r2 = AddressPR(Scrlong)			//Temp storage for a long
0x9dec1000,	//70	*r2 = r12						//Store pixel count
0x946f0174,	//71	r3 = AddressPR(myDelta)     
0x7c000807,	//72	a0 = float32(*r2)					//pixel count as a float
0x94cf0168,	//73	r6 = AddressPR(myYstart)    // Set pointer to y
0x956f0150,	//74	r11 = AddressPR(Zero)    
0x20661807,	//75	a3 = *r6 + a0 * *r3			//a3 = ystart + pixel x delta
0x946f0158,	//76	r3 = AddressPR(myXstart)    // Set pointer to x    
0x30400c07,	//77	a2 = *r3					//a2 = xstart
0x30002c07,	//78	a0 = *r11                   // Zero the a0 accumulator.     
0x30202c07,	//79	a1 = *r11                    // Zero the a1 accumulator.     
0x30400147,	//80	*r8++ = a2 = a2                // Save real c.     
0x306001c6,	//81	*r8-- = a3 = a3                // Save imaginary c.     
0x980b0020,	//82	r11 = r0                    // Zero iteration count. 
			//83	
0x128f0078,	//84	Mandel:     pccall TestMag(r18)    
0x980b5837,	//85	r11 = r11 + 1                // Increment iteration count.     
			//86	
0x808f0028,	//87	if(ne) pcgoto Pointdone        // This point diverged; it's done.     
0x80000000,	//88	nop    
			//89	
0x98e0582e,	//90	r11 - r14                    // Compare to max. num. of iterations.     
0x818f001c,	//91	if(gt) pcgoto Pointdone        // Return if greater than or equal to.     
0x80000000,	//92	nop    
			//93	
			//94	// Now the famous z = z^2 + C function 
			//95	DoTheBrot:    
0x70404007,	//96	a2 = a0 * a1    
0x70600007,	//97	a3 = a0 * a0    
0x6c804087,	//98	a0 = a3 - a1 * a1            // This gives the real part of z squared.     
0x34208107,	//99	a1 = a2 + a2                // This gives the imaginary part of z squared.     
0x3411c007,	//100	a0 = a0 + *r8++                // This is the +C (real) part of the equation.     
0x802fffc8,	//101	pcgoto Mandel    
0x34318087,	//102	a1 = a1 + *r8--                // This is the +C (img.) part of the equation. 
			//103	
			//104	Pointdone:    
0x9d6b5017,	//105	*r10++ = (short)r11            // Save result.
0x944f010c,	//106	r2 = AddressPR(Points)		//Number of pixels per column
0x9ce21000,	//107	r2 = *r2
0x980c6037,	//108	r12 = r12 + 1
0x98e0102c,	//109	r2 - r12
0x818fff74,	//110	if(gt) pcgoto Mloop
0x80000000,	//111	nop
			//112	
0x946f0108,	//113	r3 = AddressPR(flag)    
0xc004feed,	//114	r4 = 0xD1DFEED    
0x90840d1d,	//114
0x80000000,	//115	2*nop    
0x80000000,	//115
0x9de41800,	//116	*r3 = r4				//signal finished interrupt routine    
0x80000000,	//117	3*nop
0x80000000,	//117
0x80000000,	//117
0x803e0000,	//118	ireturn					//return from interrupt and wait for next initiation
0x80000000,	//119	nop
			//120	
			//121	
			//122	//**********************************************************************
			//123	// TestMag -- subroutine to compare magnitude of complex number in a0    
			//124	//        and a1 to 2.0.  If greater, r1 = 1 upon exiting; if                
			//125	//        less, r1 = 0.                                                    
			//126	//**********************************************************************
			//127	TestMag:    
0x70600007,	//128	a3 = a0 * a0                // Square real part.     
0x6c604087,	//129	a3 = a3 + a1 * a1            // Add to img. part squared.     
0x944f00a0,	//130	r2 = AddressPR(Four)    
0x34e40187,	//131	a3 = a3 - *r2                // Compare to 4.0.     
0x80000000,	//132	nop    
0x80000000,	//133	nop    
0x80000000,	//134	nop    
0x826f0004,	//135	if(alt) pcgoto Donef        // If negative result, goto Done.     
0x98010020,	//136	r1 = r0    
0x14200001,	//137	r1 = (short)1                // Result did diverge. 
			//138	Donef:    
0x80340000,	//139	return(r18)    
0x9ae10000,	//140	r1 - 0                        // Set flags based on result. 
			//141	
			//142	
			//143	//**********************************************************************
			//144	// Exception vector table -- so we can redirect interrupts to our code
			//145	//
			//146	//**********************************************************************
			//147	
0x802ffec4,	//148	Vec_tab:	if(true) pcgoto trap_unhandled	//reset
0x80000000,	//149				nop
0x802ffebc,	//150				if(true) pcgoto trap_unhandled	//bus err
0x80000000,	//151				nop
0x802ffeb4,	//152				if(true) pcgoto trap_unhandled	//illegal instr
0x80000000,	//153				nop
0x802ffeac,	//154				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//155				nop
0x802ffea4,	//156				if(true) pcgoto trap_unhandled	//addr err
0x80000000,	//157				nop
0x802ffe9c,	//158				if(true) pcgoto trap_unhandled	//DSU over/underflow
0x80000000,	//159				nop
0x802ffe94,	//160				if(true) pcgoto trap_unhandled	//NaN
0x80000000,	//161				nop
0x802ffe8c,	//162				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//163				nop
0x802ffe84,	//164				if(true) pcgoto trap_unhandled	//Int 0
0x80000000,	//165				nop
0x802ffe7c,	//166				if(true) pcgoto trap_unhandled	//Timer
0x80000000,	//167				nop
0x802ffe74,	//168				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//169				nop
0x802ffe6c,	//170				if(true) pcgoto trap_unhandled	//Boot ROM
0x80000000,	//171				nop
0x802ffe64,	//172				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//173				nop
0x802ffe5c,	//174				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//175				nop
0x802ffe54,	//176				if(true) pcgoto trap_unhandled	//reserved
0x80000000,	//177				nop
0x802ffe70,	//178				if(true) pcgoto trap_int	//Int 1
0x9d600408,	//179				emr = (short) r0			//mask further ints
			//180	
			//181	//**********************************************************************
			//182	// Processing, input and output data, and flags
			//183	//
			//184	//**********************************************************************
			//185	
0x00000084,	//186	Four:        float 16.0            // Number for magnitude comparisons. 
0x00000000,	//187	Zero:        float 0.0
0x00000000,	//188	Scrfl:        float 0.0            // Scratch pad. 
0x00000000,	//189				float 0.0
0x00000000,	//190	Scrlong:	long 0
			//191	
0x00000000,	//192	myXstart:    float    0.0
0x00000000,	//193	myYstart:    float    0.0
0x00000000,	//194	myDelta:	float	0.0
0x00000100,	//195	Points:		long 256
			//196	
			//197	// Note: Xstart and Ystart all hold IEEE representations of    
			//198	// floating point values.  Hence, they have been declared as longs    
			//199	// in order that they not be confused with DSP floats.                
			//200	
0x0000003f,	//201	Maxiter:    long    63        // Maximum number of iterations. 
0x00000050,	//202	Xstart:        long    80        // Starting real (x) coordinate. 
0x00000050,	//203	Ystart:        long    80        // y coordinate.
0x00000001,	//204	Delta:		long 1			//addition to y coord for each pixel 
			//205	
0x00000000,	//206	flag:        long    0
			//207	OutputData:     // Output either maxiter or not 
			//8 * 16 * long = 256 shorts = 1 column of pixels
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


#define Maxiter 0x214
#define XStart 0x218
#define YStart 0x21c
#define delta 0x220
#define flag 0x224
#define OutputData 0x228
#define outputlen 8*16

//**************************************************************************
//	Globals
//**************************************************************************

typedef struct out_params
{
	volatile long Flag;
	volatile short Outputdata[255];
} out_params;

typedef struct in_params
{
	volatile long MaxIter;
	volatile float Xstart;
	volatile float Ystart;
	volatile float Delta;
} in_params;


extern struct CIA __far ciaa;
/* DSP control registers. */

ULONG * volatile dsp_read = (void *) 0x00dd005C;
UBYTE * volatile dsp_write = (void *) 0x00dd0080;
ULONG * volatile zero = NULL;

volatile struct out_params *output_v = (struct out_params *) &dspcode[flag>>2];
struct in_params *input_v = (struct in_params *) &dspcode[Maxiter>>2];;

int processor = 2;
int compatibility = 1;
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
		switch(compatibility)
		{
		case 0:
			*dsp_write = (UBYTE)(val & 0xff);
			break;

		case 1:
			for(i = 0; i<3; i++) *dsp_write = (UBYTE)(val & 0xff);
			break;
		}

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
	//cache_curr = CacheControl(0x0, cache_mask);	//turn off data cache
	int i;
	output_v->Flag = 0x0;
	SetCtrl(0xfd); /* cause int1 on dsp */
	
	for(i=0; i<10; i++) {
		if(output_v->Flag == 0xD1DFEED) return;
	}


	//if(output_v->Flag != 0xD1DFEED) {
		printf(" ** DSP failed to return magic cookie: 0x%08x\n",output_v->Flag);
		//exit(EXIT_FAILURE);
	//}
	//cache_curr = CacheControl(cache_recall, cache_mask);	//restore cache 
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
		printf("Usage mandel_dsp, with -c for compatibility mode\n");
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
	if(argc==2)
		{
			printf("argument is %s\n",argv[0]);
		if(!strcasecmp(argv[1],"-c")) 
		{ compatibility=1; } 
		else 
		{ printf("Usage mandel_dsp, with -c for compatibility mode\n");
		exit(EXIT_FAILURE);
		}
	}

	else	/* no arguments given */
		{
		x1=-2.2;
		x2=1.1;
		y11=-1.4;
		y2=1.4;
		maxcount=50;
		}
		
	printf("Original code by Erik Trulsson 1996.\n");
	printf("DSP coding Wrangler Jan 2021 based on ARTABROT by George T Warner\n");
	printf("Version 2.05\n");
	printf("Usage: double click to zoom, esc or q quits, Z zoom out\n");
	printf("d use DSP, f use FPU, c to toggle DSP compatibility mode\n");

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

	//cache_recall = 
	//cache_orig = CacheControl(0x0, 0x0);		//read initial cache states

	*zero = (LONG) dspcode; /* dsp reads PC addr from here after int1 */
	InitDSP(); /* Reset and set up DSP */

	output_v->Flag = 0;
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
						//printf("compatibility flag is %d\n", compatibility);
						//if(compatibility) cache_curr = CacheControl(0x0, cache_mask);	//turn off data cache
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						//exit(EXIT_FAILURE); /* should never reach here */
					case 'f':
					case 'F':
						processor = 2;
						//cache_curr = CacheControl(cache_orig, cache_mask);	//restore cache 
						ReplyMsg((struct Message *)msg);
						longjmp(jb,1);
						//exit(EXIT_FAILURE); /* should never reach here */
					case 'c':
					case 'C':
						compatibility = 1-compatibility;
						printf("Compatibility mode is now %d\n", compatibility);
						ReplyMsg((struct Message *)msg);
						return;
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

	am=(x2-x1)/width;
	bm=(y11-y2)/height;
	
	for(i=0,a=x1;i<width;i++)	
		{
		switch(processor)
			{
			case 1:
				b=y2;
				input_v->Xstart = a;
				input_v->Ystart = b;
				input_v->Delta = bm;
				//output_v->Outputdata = 0;

				cachelen = 4*(256+5);
				CachePreDMA(&dspcode[Maxiter>>2],&cachelen,0);
				DSP_int();	//DSP processes a column
				cachelen = 4*(256+5);
				CachePostDMA(&dspcode[Maxiter>>2],&cachelen,0);

				for(j=0;j<height;)		//colour pixels
					{
					HandleMsg();
					for(j1=0;j1<8 && j<height;j1++,j++)
						{
						if(output_v->Outputdata[j]-2>=maxcount)
							{
							SetAPen(rp,1);
							}
						else
							{
							SetAPen(rp,(output_v->Outputdata[j]-2) % (1<<depth));
							}
						WritePixel(rp,i,j);
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
	}


	