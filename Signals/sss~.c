/*
	File:		sss~.c

	Contains:	"Pink" noise using the Voss/Gardner (simplified) algorithm.

	Written by:	Peter Castine

	Copyright:	© 2001-2002 Peter Castine

	Change History (most recent first):

         <6>   24–3–2006    pc      Update #includes for new LitterLib organization.
         <5>     14–1–04    pc      Update for Windows.
         <4>    6–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <3>  30–12–2002    pc      Drop faux 'Vers' resource and related modifications. Add object
                                    version to SssInfo().
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

const char*			kClassName		= "lp.sss~";			// Class name

#define LPAssistIn1			"Int (NN factor)"
#define LPAssistOut1		"Signal (Pink noise)"



#ifdef __MWERKS__
	const int			kArraySize		= 32,
						kMaxNN			= 31;
#else
	// This is for the benefit of lame GCC that can't handle
	// const ints as array size specifiers
	#define kArraySize	32
	#define kMaxNN		31
#endif
/*
	// Indices for STR# resource
enum {
	strIndexInNN		= lpStrIndexLastStandard + 1,
	
	strIndexOutPink
	};

	// Indices for MSP Inlets and Outlets
	// 
enum {
	inletPink			= 0,		// For begin~
	
	outletPink
	};
 */

#pragma mark • Type Definitions



#pragma mark • Object Structure

typedef struct {
	t_pxobject		coreObject;
	
	unsigned short	counter;
	int				nn;					// Number of bits to mask out
    /*
	unsigned long	mask,				// Value depends on nn
					offset,				// ditto
					sum,
					dice[kArraySize];
     */
    t_uint32        mask,				// Value depends on nn
                    offset,				// ditto
                    sum,
                    dice[kArraySize];
	} tPink;


#pragma mark • Global Variables



#pragma mark • Function Prototypes

	// Class message functions
void*	SssNew(long);

	// Object message functions
static void SssNN(tPink*, long);
static void SssTattle(tPink*);
static void	SssAssist(tPink*, void* , long , long , char*);
static void	SssInfo(tPink*);

	// MSP Messages
//static void	SssDSP(tPink*, t_signal**, short*);
//static int*	SssPerform(int*);
void SssDSP64(tPink*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void SssPerform64(tPink*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


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
			(method) SssNew,			// Instance creation function
			(method) dsp_free,			// Default deallocation function
			sizeof(tPink),				// Class object size
			NIL,						// No menu function
			A_DEFLONG,					// Optional arguments:	1. NN Factor
			0);		
	
	//dsp_initclass();
        class_dspinit(c);

	// Messages
	class_addmethod(c, (method) SssNN,      "int",      A_LONG, 0);
	class_addmethod(c, (method) SssTattle,	"dblclick",	A_CANT, 0);
	class_addmethod(c, (method) SssTattle,	"tattle",	A_NOTHING);
	class_addmethod(c, (method) SssAssist,	"assist",	A_CANT, 0);
	class_addmethod(c, (method) SssInfo,	"info",		A_CANT, 0);
	
	// MSP-Level messages
	class_addmethod(c, (method) SssDSP64,	"dsp64",	A_CANT, 0);

	//Initialize Litter Library
	//LitterInit(kClassName, 0);
        class_register(CLASS_BOX, c);
        gObjectClass = c;
	Taus88Init();
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;
	
	}



#pragma mark -
#pragma mark • Class Message Handlers

/******************************************************************************************
 *
 *	SssNew(iNN)
 *	
 *	Arguments: nn factor.
 *
 ******************************************************************************************/

	static void InitDice(tPink* me)
		{
		int				i		= kArraySize;
		t_uint32	sum		= 0;
		t_uint32*	curDie	= me->dice;
		
		do { sum += *curDie++ = Taus88(NULL) / kArraySize; }
			while (--i > 0);
		
		me->sum = sum;
		
		}

void*
SssNew(
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
	SssNN(me, iNN);
	me->counter = 0;
	InitDice(me);

	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	SssNN(me, iNN)
 *	
 *	Set parameters, making sure nothing bad happens.
 *
 ******************************************************************************************/

void SssNN(
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
 *	SssTattle(me)
 *
 *	Post state information
 *
 ******************************************************************************************/

void
SssTattle(
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
 *	SssAssist()
 *	SssInfo()
 *
 *	Fairly generic Assist/Info methods
 *
 ******************************************************************************************/

void SssAssist(tPink* me, void* box, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexInNN, strIndexOutPink, oCStr);
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

void SssInfo(tPink* me)
	{ LitterInfo(kClassName, &me->coreObject.z_ob, (method) SssTattle); }

#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	SssDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/
/*
void
SssDSP(
	tPink*		me,
	t_signal**	ioDSPVectors,
	short*		connectCounts)
	
	{
	#pragma unused(connectCounts)
	
	dsp_add(
		SssPerform, 3,
		me, (long) ioDSPVectors[outletPink]->s_n, ioDSPVectors[outletPink]->s_vec
		);
	
	}*/

void SssDSP64(tPink* me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags) {
    
    object_method(dsp64, gensym("dsp_add64"), me, SssPerform64, 0, NULL);
}

void SssPerform64(tPink* me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
	{
        
	long			vecCounter;
	tSampleVector	outNoise;
	//unsigned long*	firstDie;
    t_uint32*       firstDie;
	t_uint32        sum,            // unsigned long
					mask,
					offset;
	unsigned short	counter;				// Must be 16-bit value to match "dice" array
	
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
		//unsigned long*		curDie		= firstDie;
        t_uint32*           curDie		= firstDie;
		long				bitCount	= CountBits(counter);
		unsigned			bit			= 0x01;
		
		switch (bitCount) {
			case 0:
				InitDice(me);
				sum = me->sum;
				break;
			
			case 1:
				// Just update one die
				while ((bit & counter) == 0)
					{
					bit		*= 2;			// Let compiler choose fastest way to do this
					curDie	+= 1;
					}
				sum -= *curDie;
				sum += *curDie = Taus88(NULL) / kArraySize;
				break;
			
			default:
				if (bitCount < kArraySize / 2) {
					// Still faster to calculate differences
					int i = kArraySize;
					do {
						if (bit & counter) {
							sum -= *curDie;
							sum += *curDie = Taus88(NULL) / kArraySize;
							}
						bit		*= 2;
						curDie	+= 1;
						} while (--i > 0);
					}
				else {
					// Faster just to update and recalculate sum from scratch
					int i	= kArraySize;
					sum	= 0;
					do {
						if (bit & counter)
							*curDie = Taus88(NULL) / kArraySize;
						sum += *curDie++;
						bit *= 2;
						} while (--i > 0);
					}
				break;
			}
		
		*outNoise++	= ULong2Signal((sum & mask) + offset);
		counter++;
		}
		while (--vecCounter > 0);
	
	me->sum		= sum;
	me->counter	= counter;
	
	}
