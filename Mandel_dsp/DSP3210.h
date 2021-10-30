//**************************************************************************
//  DSP3210.h
//	Support routines for the DSP - header file
//	By Wrangler 2021
//  Use this code however you like but you must credit me 
//**************************************************************************

#ifndef DSP3210_H
#define DSP3210_H

#include <hardware/cia.h>

//Initialise the DSP and commence execution at address dspcode
//with dspflag as the address that will signal initialisation is complete
void DSP_init(ULONG dspcode);

//Close down using the DSP
void DSP_exit();

//Trigger an interrupt on the DSP
//having cleared caches on the input data first
//input = address from which the DSP reads inputs
//inputlen = length of input in bytes
void DSP_int(ULONG input, ULONG inputlen);

//Wait for the DSP to signal ready
//dspflag = address used by the DSP to signal
//output = address from which the DSP uses to store results
//outputlen = length of output in bytes
//returns 0 if DSP is ready, and clears cache of output area
//returns 1 if DSP busy after a reasonable period
int DSP_waitready(ULONG output, ULONG outputlen);

#endif