/*
	File:		zzz~.c

	Contains:	"Pink" noise, using the McCartney algorithm.

	Written by:	Peter Castine

	Copyright:	© 2001-2002 Peter Castine

	Change History (most recent first):

         <6>   24–3–2006    pc      Update #includes for new LitterLib organization.
         <5>     14–1–04    pc      Update for Windows.
         <4>    6–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <3>  30–12–2002    pc      Drop faux 'Vers' resource and related modifications. Add object
                                    version to ZzzInfo().
         <2>  28–11–2002    pc      Tidy up after initial check in.
         <1>  28–11–2002    pc      Initial check in.
		14-Apr-2001:		First implementation.
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"
//#include "TrialPeriodUtils.h"
#include "Taus88.h"
#include "MiscUtils.h"


#pragma mark • Constants

const char*			kClassName		= "lp.zzz~";			// Class name

#define LPAssistIn1			"Int (NN factor)"
#define LPAssistOut1		"Signal (Pink noise)"

const char* lpversion = "64-bit version. Copyright 2001-08 Peter Castine, Part of Litter Power 1.8";

#ifdef __GNUC__
	// Lame
	#define		kArraySize		16
	#define		kMaxNN			31
#else
	const int	kArraySize		= 16,
				kMaxNN			= 31;
#endif
/*
	// Indices for STR# resource
enum {
	strIndexInNN		= lpStrIndexLastStandard + 1,
	strIndexOutPink,
	
	strIndexLeftInlet	= strIndexInNN,
	strIndexLeftOutlet	= strIndexOutPink
	};
*/
#pragma mark • Type Definitions



#pragma mark • Object Structure

typedef struct {
	t_pxobject		coreObject;
	
	int				nn;					// Number of bits to mask out
	t_uint32        mask,				// Value depends on nn
					offset,				// ditto
					sum,
					dice[kArraySize];
	unsigned short	counter;
	} tPink;


#pragma mark • Global Variables



#pragma mark • Function Prototypes

	// Class message functions
void*	ZzzNew(long);

	// Object message functions
static void ZzzNN(tPink*, long);
static void ZzzTattle(tPink*);
static void	ZzzAssist(tPink*, void* , long , long , char*);
static void	ZzzInfo(tPink*);

	// MSP Messages
//static void	ZzzDSP(tPink*, t_signal**, short*);
//static int*	ZzzPerform(int*);
void ZzzDSP64(tPink*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void ZzzPerform64(tPink*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


#pragma mark -
/*****************************  I M P L E M E N T A T I O N  ******************************/

#pragma mark • Inline Functions



#pragma mark -

/******************************************************************************************
 *
 *	main()
 *	
 *	Standar Max/MSP External Object Entry Point Function
 *	
 ******************************************************************************************/

int C74_EXPORT main(void)
	
	{
	//LITTER_CHECKTIMEOUT(kClassName);
        t_class *c;
	
	// Standard Max/MSP initialization mantra
	c = class_new(kClassName,
			(method) ZzzNew,			// Instance creation function
			(method) dsp_free,			// Default deallocation function
			sizeof(tPink),				// Class object size
			NIL,						// No menu function
			A_DEFLONG,					// Optional arguments:	1. NN Factor
			0);		
	
	//dsp_initclass();
        class_dspinit(c);

	// Messages
	class_addmethod(c, (method) ZzzNN,      "int",      A_LONG, 0);
	class_addmethod(c, (method) ZzzTattle,	"dblclick",	A_CANT, 0);
	class_addmethod(c, (method) ZzzTattle,	"tattle",	A_NOTHING);
	class_addmethod(c, (method) ZzzAssist,	"assist",	A_CANT, 0);
	class_addmethod(c, (method) ZzzInfo,	"info",		A_CANT, 0);
	
	// MSP-Level messages
	class_addmethod(c, (method) ZzzDSP64,	"dsp64",	A_CANT, 0);

	//Initialize Litter Library
	//LitterInit(kClassName, 0);
        class_register(CLASS_BOX, c);
        gObjectClass = c;
	Taus88Init();
        
        post("%s: %s", kClassName, lpversion);
        return 0;
	
	}



#pragma mark -
#pragma mark • Class Message Handlers

/******************************************************************************************
 *
 *	ZzzNew(iNN)
 *	
 *	Arguments: nn factor.
 *
 ******************************************************************************************/

	static void InitDice(tPink* me)
		{
		int				i		= kArraySize;
		//unsigned long	sum		= 0;
        t_uint32	sum		= 0;
		t_uint32*	curDie	= me->dice;
		
		do { sum += *curDie++ = Taus88(NULL) / kArraySize; }
			while (--i > 0);
		
		me->sum = sum;
		
		}

void*
ZzzNew(
	long	iNN)
	
	{
	tPink*	me	= NIL;
	
	// Default NN value doesn't need massaging

	// Let Max/MSP allocate us, our inlets, and outlets.
	me = object_alloc(gObjectClass);
	dsp_setup(&(me->coreObject), 1);				// Signal inlet for benefit of begin~
													// Otherwise left inlet does "NN" only
	outlet_new(me, "signal");
	
	// Set up object components
	ZzzNN(me, iNN);
	me->counter = 0;
	InitDice(me);

	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	ZzzNN(me, iNN)
 *	
 *	Set parameters, making sure nothing bad happens.
 *
 ******************************************************************************************/

void ZzzNN(
	tPink* me,
	long	iNN)
	
	{
	
	if (iNN <= 0) {
		me->nn		= 0;
		me->mask	= kULongMax;
		me->offset	= 0;
		}
	else {
		if (iNN > kMaxNN)
			iNN = kMaxNN;
		me->nn		= (t_uint32)iNN;
		me->mask	= kULongMax << iNN;
		me->offset	= (~me->mask) >> 1;
		}
	
	}


/******************************************************************************************
 *
 *	ZzzTattle(me)
 *
 *	Post state information
 *
 ******************************************************************************************/

void
ZzzTattle(
	tPink* me)
	
	{
	post("%s state",
			kClassName);
	post("  NN factor is: %d (mask = 0x%lx)",
			me->nn,
			me->mask);
	post("  Current sum is: %lu",
			me->sum);
	}


/******************************************************************************************
 *
 *	ZzzAssist()
 *	ZzzInfo()
 *
 *	Fairly generic Assist/Info methods
 *
 ******************************************************************************************/

void ZzzAssist(tPink* me, void* box, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexLeftInlet, strIndexLeftOutlet, oCStr);
        if (iDir == ASSIST_INLET) {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistIn1); break;
            }
        }
        else {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistOut1); break;
                    //case 1: sprintf(s, "..."); break;
            }
            
        }
	}

void ZzzInfo(tPink* me)
	{ LitterInfo(kClassName, &me->coreObject.z_ob, (method) ZzzTattle); }


#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	ZzzDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/

void ZzzDSP64(tPink* me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags) {
    
    object_method(dsp64, gensym("dsp_add64"), me, ZzzPerform64, 0, NULL);
    
}

void ZzzPerform64(tPink* me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
	{
	
	long			vecCounter;
	tSampleVector	outNoise;
	//tPink*			me = (tPink*) iParams[paramMe];
	//unsigned long*	firstDie;
    t_uint32*	firstDie;
	t_uint32	sum,
					mask,
					offset;
	unsigned short	counter;						// Must be 16-bit to match dice count
	
	if (me->coreObject.z_disabled) return;
	
	// Copy parameters into registers
	vecCounter	= sampleframes;
	outNoise	= outs[0];
	sum			= me->sum;
	firstDie	= me->dice;
	mask		= me->mask;
	offset		= me->offset;
	counter		= me->counter;
	
	// Do our stuff
	do {
		//unsigned long	samp;
        t_uint32	samp;
		
		if (counter != 0) {
			// Need to count the number of clear LSBs
			t_uint32	rightZeroes	= 0;
			unsigned 		testBit		= 0x01;
			t_uint32*	curDie;				// Calculate address later...
			
			// ASSERT: counter != 0
			// (Otherwise the following will never terminate!)
			while ((counter & testBit) == 0) {
				testBit		*= 2;
				rightZeroes	+= 1;
				}
			
			// rightZeroes is the index into the array of "dice" of the element that gets modified
			// this time around. Calculate the address once
			curDie = firstDie + rightZeroes;
			sum -= *curDie;
			sum += *curDie = Taus88(NULL) / kArraySize;
			}
		// If c == 0, nothing gets changed (aside from the hi-freq. white noise component)
		counter++;
		
		// Add high frequency white-noise component to sum
		samp  = Taus88(NULL) / kArraySize;
		samp += sum;
		samp &= mask;
		*outNoise++	= ULong2Signal(samp + offset);
		}
		while (--vecCounter > 0);
	
	me->sum		= sum;
	me->counter	= counter;
	
	}
