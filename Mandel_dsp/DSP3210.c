//**************************************************************************
//	DSP3210.c
//	Support routines for the DSP - code file
//	By Wrangler 2021
//  Use this code however you like but you must credit me 
//**************************************************************************

#include <stdlib.h>
#include <stdio.h>
#include <proto/exec.h>
#include <exec/types.h>
#include <exec/memory.h>

#include <DSP3210.h>

extern struct CIA __far ciaa;

extern volatile ULONG DSP_flag;

/* Defines for Flag */
#define DSP3210_OK		0xC0FFEE
#define DSP3210_RUN		0xD0FEED
#define DSP3210_READY	0xD1DFEED
#define DSP3210_FAIL	0xDEADDEAD

/* Defines for DSP control registers */
#define DSP3210_RREG	0x00dd005C
#define DSP3210_WREG	0x00dd0080

#define DSP3210_NORM	0xFF
#define DSP3210_RSET	0x7F
#define DSP3210_INT1	0xFD

/* DSP control registers. */

ULONG * volatile dsp_read = (void *) DSP3210_RREG;
UBYTE * volatile dsp_write = (void *) DSP3210_WREG;
ULONG * volatile zero = NULL;

void SetCtrl(ULONG val);
BOOL WakeupWait(ULONG dspcode);

void SetCtrl(ULONG val) 
{
	short i;
	ULONG mask = 0x4c;

	do {
		*dsp_write = (UBYTE)(val & 0xff);

		for (i = 0; i<256; ++i);
		if ((*dsp_read & mask) == (val & mask)) break;
		printf("**Control Write Failure: $%2x != $%2x\n",(int)(*dsp_read & mask),(int)(val & mask));
	} while (TRUE);
}

#define BASICWAIT 100000
#define RETRYTHRESHOLD 10

BOOL WakeupWait(ULONG dspcode) 
{
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

void DSP_init(ULONG dspcode) {
	long i, j;

	*zero = (LONG) dspcode; /* dsp reads PC addr from here after int1 */

	SetCtrl(DSP3210_RSET); /* Set up for DSP in reset */

	for (i = 0; i < 1000; i++) 
		j = ciaa.ciapra;

	SetCtrl(DSP3210_NORM); /* Take DSP out of reset */
	SetCtrl(DSP3210_INT1); /* cause int1 on dsp */
	if (!WakeupWait(dspcode)) { /* Wait for DSP to wake up */
		printf("*** DSP failed to wakeup\n");
		exit(EXIT_FAILURE);
	}
	
	if(DSP_flag != DSP3210_OK) {
		printf("*** DSP failed to initialise and return magic number: 0x%08x\n",(int)DSP_flag);
		exit(EXIT_FAILURE);
	}

	return;
}

void DSP_exit() 
{
	*zero = 0;
	SetCtrl(DSP3210_NORM);
	return;
}

void DSP_int(ULONG input, ULONG inputlen) 
{	
	ULONG cachelen = 4;
	CachePreDMA(&DSP_flag,&cachelen,0);
	CachePreDMA(&input,&inputlen,0);
	SetCtrl(DSP3210_INT1); /* cause int1 on dsp */
	return;
}

int DSP_waitready(ULONG output, ULONG outputlen) 
{
	int i;
	long j;
	//volatile long v;
	ULONG cachelen = 4;
	CachePostDMA(&DSP_flag,&cachelen,0); //clear read cache for flag

	if(DSP_flag ==  DSP3210_OK)
		return 0; //got initialised signal

	for(i=0; i<BASICWAIT/10; i++) {
		cachelen = 4;
		CachePostDMA(&DSP_flag,&cachelen,0); //clear read cache for flag
		if(DSP_flag ==  DSP3210_READY) {
			cachelen = outputlen;
			CachePostDMA(&output,&cachelen,0);	//clear entire data area
			DSP_flag = DSP3210_OK;
			return 0; //got ready signal
		}
		j = ciaa.ciapra;
	}
	return 1;
}
