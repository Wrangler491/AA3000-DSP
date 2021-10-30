//**************************************************************************
//	Mandel_DSP by Wrangler, October 2021
//	Mandelbrot render program for the Amiga AA3000 or AA3000+ with DSP3210
//	Based on Mandel by Erik Trulsson 1996 and on ARTABROT by George T Warner
//	Use this code however you like but you must credit me and the two other
//  authors as above
//**************************************************************************


#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <devices/timer.h>

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

#include <DSP3210.h>

int version = 12;


//**************************************************************************
//	DSP labels
//**************************************************************************
extern int dspcode;
extern int Maxiter;
extern float XStart;
extern float YStart;
extern float Delta;
extern ULONG *Buffer;  //OutputData
extern ULONG OutputData1;
extern ULONG OutputData2;

#define outputlen 8*16	//measured in longs
#define paramlen 4


//**************************************************************************
//	Globals
//**************************************************************************

int processor = 2;

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
struct DrawInfo  *drawinfo;

void Cleanup(void);
int HandleMsg(void);
ULONG secs,msecs,olds=0,oldms=0;
void zoomin(WORD,WORD);
void zoomout(WORD,WORD);
int drawmandel(void);
double x1,x2,y11,y2;
int maxcount;
int oldpri=0;

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
	int timetaken;
	int timefrac;

	if(atexit(Cleanup)) return EXIT_FAILURE;

	if(argc!=2 && argc!=1)
		{
		printf("Usage mandel_dsp\n");
		exit(EXIT_FAILURE);
		}
		
	else	/* no arguments given */
		{
		x1=-2.2f;
		x2=1.1f;
		y11=-1.4;
		y2=1.4;
		maxcount=50;
		}
		
	printf("Original code by Erik Trulsson 1996.\n");
	printf("DSP coding Wrangler Oct 2021 based on ARTABROT by George T Warner\n");
	printf("Version 3.%d\n", (int)version);
	printf("Usage: double click to zoom, esc or q quits, Z zoom out\n");
	printf("d use DSP, f use FPU\n");

	if(OpenDevice("timer.device", 0, &timereq, 0)) exit(EXIT_FAILURE);
	TimerBase = timereq.io_Device;
			
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

	
	DSP_init((ULONG)&dspcode); /* Reset and set up DSP */

	Maxiter = maxcount+1;

	rp=win->RPort;
	mp=win->UserPort;
	oldpri=SetTaskPri(FindTask(0),-1);
	
	for(i=0;i<=11;i++)
		{
		SetRGB4(&(scrn->ViewPort),i+20,ct[i][0],ct[i][1],ct[i][2]);
		}

	while(1)
		{
		SetRast(rp,0);
		GetSysTime(&start);
		if(!drawmandel()) {

			GetSysTime(&end);
			SubTime(&end, &start);
			timetaken = end.tv_secs;
			timefrac = (int)(end.tv_micro/10000.f);

			switch(processor)
			{
			case 1:
				snprintf(buffer, 30, "DSP took %d.%d seconds", timetaken, timefrac);
				break;

			case 2:
				snprintf(buffer, 30, "FPU took %d.%d seconds", timetaken, timefrac);
				break;
			}

			myIText.IText = buffer;
	
			PrintIText(win->RPort,&myIText,10,10);

			do {
				WaitPort(mp);
				} while (!HandleMsg());
			}
		}	
	DSP_exit();
	
	return 0;
	}

	
void Cleanup(void)	
	{
	if(drawinfo!=NULL) {FreeScreenDrawInfo(scrn,drawinfo); drawinfo = NULL;}
	if(win!=NULL) {CloseWindow(win); win = NULL;}
	if(scrn!=NULL) {CloseScreen(scrn); scrn = NULL;}
	if(GfxBase!=NULL) {CloseLibrary((struct Library *)GfxBase); GfxBase = NULL;}
	if(IntuitionBase!=NULL) {CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL;}
	if(TimerBase!=NULL) {CloseDevice(&timereq); TimerBase = NULL;}
	SetTaskPri(FindTask(NULL),oldpri);
	DSP_exit();
	}

//**************************************************************************
//	Intuition message handler
//**************************************************************************
	
int HandleMsg(void)
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
						return 1;
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
						return 1;
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
						exit(0);
					case 'z':
						zoomin(width/2,height/2);
						ReplyMsg((struct Message *)msg);
						return 1;
					case 'Z':
						zoomout(width/2,height/2);
						ReplyMsg((struct Message *)msg);
						return 1;
					case 'd':
					case 'D':
						processor = 1;
						ReplyMsg((struct Message *)msg);
						return 1;
					case 'f':
					case 'F':
						processor = 2;
						ReplyMsg((struct Message *)msg);
						return 1;
					}
			}				
		ReplyMsg((struct Message *)msg);
		}
	return 0;
	}


//**************************************************************************
//	Render routines 
//**************************************************************************

void zoomin(WORD x,WORD y)	
	{
	double nx,ny;
	double dx,dy;
	maxcount+=25;
	Maxiter = maxcount+1;
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
	Maxiter = maxcount+1;
	dx=(x2-x1);	
	dy=(y2-y11);
	
	nx=(dx*x)/width+x1;
	ny=(dy*(height-y))/height+y11;
	x1=nx-(dx/2)*2;
	x2=nx+(dx/2)*2;
	y11=ny-(dy/2)*2;
	y2=ny+(dy/2)*2;	
	}

int drawmandel(void)
	{
	int i,j,j1;
	register double a,b,am,bm;
	register double u,v,w,x,y;

	int k;
	ULONG *outptr;
	short *sp;

	am=(x2-x1)/width;
	bm=(y11-y2)/height;

	for(i=0,a=x1;i<width;i++)	
		{
		switch(processor)
			{
			case 1:
				b=y2;
				outptr = ((i & 1) == 0 )? &OutputData1 : &OutputData2;	//double buffer 
				if(DSP_waitready((ULONG)&OutputData1,4*(outputlen*2)))	//don't set up the DSP until it has finished processing
					printf(" ** DSP failed to return ready cookie in a reasonable amount of time\n");
				XStart = (float) a;
				YStart = (float) b;
				Delta =  (float) bm;
				Buffer = outptr;

				DSP_int((ULONG)&Maxiter,4*(paramlen));	//DSP processes a column

				if(i) {	//skip for first iteration
					sp = ((i & 1) == 0)? (short *)&OutputData2 : (short *)&OutputData1;
					for(j=0;j<height;)		//colour pixels
						{
						if(HandleMsg()) return 1;
						for(j1=0;j1<8 && j<height;j1++,j++)
							{
							if( *(sp+j) -2 >= maxcount)
								SetAPen(rp,1);
							else
								SetAPen(rp,(*(sp+j) -2) % (1<<depth));
							WritePixel(rp,i-1,j);	//colour previous column
							}
						}
				}
				break;
					
			case 2:
				for(j=0,b=y2;j<height;)
					{
					if(HandleMsg()) return 1;
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
							SetAPen(rp,1);
						else
							SetAPen(rp,k % (1<<depth));
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
		sp = ((width & 1) == 0)? (short *)&OutputData2 : (short *)&OutputData1;
		if(DSP_waitready((ULONG)&OutputData1,4*(outputlen*2)))	//wait for final column to be ready
			printf(" ** DSP failed to return ready cookie in a reasonable amount of time\n");
		for(j=0;j<height;)		//colour pixels
		{
			if(HandleMsg()) return 1;
			for(j1=0;j1<8 && j<height;j1++,j++)
			{
				if( *(sp+j) -2 >=maxcount)
					SetAPen(rp,1);
				else
					SetAPen(rp,(*(sp+j) -2) % (1<<depth));
				WritePixel(rp,width-1,j);	//colour final column
			}
		}
	}
	return 0;
}