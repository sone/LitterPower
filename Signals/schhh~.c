/*
	File:		schhh~.c

	Contains:	"White" noise. Based on Taus88 algorithm, also has unique NN factor.

	Written by:	Peter Castine

	Copyright:	© 2001-2002 Peter Castine

	Change History (most recent first):

         <6>   24–3–2006    pc      Update #includes for new LitterLib organization.
         <5>     14–1–04    pc      Update for Windows.
         <4>    6–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <3>  30–12–2002    pc      Drop faux 'Vers' resource and related modifications. Add object
                                    version to ShhhInfo().
         <2>  28–11–2002    pc      Tidy up after initial check in.
         <1>  28–11–2002    pc      Initial check in.
		1-Jun-2001:		Use Taus88Vector and Taus88FPVector to speed things up a bit.
		14-Apr-2001:	First implementation.
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"	// Also #includes MaxUtils.h, ext.h
//#include "TrialPeriodUtils.h"
#include "Taus88.h"


#pragma mark • Constants

const char*	kClassName		= "lp.shhh~";			// Class name

#define LPAssistIn1			"Int (NN factor)"
#define LPAssistOut1		"Signal (White noise)"

const int	kMaxNN			= 31;

/*
	// Indices for STR# resource
enum {
	strIndexInNN		= lpStrIndexLastStandard + 1,
	
	strIndexOutWhite
	};

	// Indices for MSP Inlets and Outlets.
enum {
	inletWhite			= 0,		// For begin~
	
	outletWhite
	};
*/
#pragma mark • Type Definitions



#pragma mark • Object Structure

typedef struct {
	t_pxobject		coreObject;
	
	int				nn;					// Number of bits to mask out
	//unsigned long	mask,				// Values depend on nn
	//				offset;
    t_uint32        mask,
                    offset;
	} tWhite;


#pragma mark • Global Variables



#pragma mark • Function Prototypes

	// Class message functions
void*	ShhhNew(long);

	// Object message functions
static void ShhhNN(tWhite*, long);
static void ShhhTattle(tWhite*);
static void	ShhhAssist(tWhite*, void* , long , long , char*);
static void	ShhhInfo(tWhite*);

	// MSP Messages
//static void	ShhhDSP(tWhite*, t_signal**, short*);
//static int*	ShhhPerform(int*);
void ShhhDSP64(tWhite*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void ShhhPerform64(tWhite*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


#pragma mark -
/*****************************  I M P L E M E N T A T I O N  ******************************/

#pragma mark • Inline Functions



#pragma mark -

/******************************************************************************************
 *
 *	main()
 *	
 *	Standard Max/MSP External Object Entry Point Function
 *	
 ******************************************************************************************/

int C74_EXPORT main(void)
	
	{
	//LITTER_CHECKTIMEOUT(kClassName);
	t_class *c;
        
	// Standard Max/MSP initialization mantra
	c = class_new(kClassName,
			(method) ShhhNew,			// Instance creation function
			(method) dsp_free,			// Default deallocation function
			sizeof(tWhite),				// Class object size
			NIL,						// No menu function
			A_DEFLONG,					// Optional arguments:	1. NN Factor
			0);		
	
	//dsp_initclass();
        class_dspinit(c);

	// Messages
	//class_addmethod(c, (method) ShhhNN);    // something is missing here, no?
    class_addmethod(c, (method) ShhhNN,     "int",      A_LONG, 0);
	class_addmethod(c, (method) ShhhTattle,	"dblclick",	A_CANT, 0);
	class_addmethod(c, (method) ShhhTattle,	"tattle",	A_NOTHING);
	class_addmethod(c, (method) ShhhAssist,	"assist",	A_CANT, 0);
	class_addmethod(c, (method) ShhhInfo,	"info",		A_CANT, 0);
	
	// MSP-Level messages
	class_addmethod(c, (method) ShhhDSP64,	"dsp64",	A_CANT, 0);

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
 *	ShhhNew(iNN)
 *	
 ******************************************************************************************/

void*
ShhhNew(
	long	iNN)
	
	{
	tWhite*		me	= NIL;
	
	// Default NN value doesn't need massaging

	// Let Max/MSP allocate us, our inlets, and outlets.
	//me = (tWhite*) newobject(gObjectClass);
      me = object_alloc(gObjectClass);
	dsp_setup(&(me->coreObject), 1);				// Signal inlet for benefit of begin~
													// Otherwise left inlet does "NN" only
	outlet_new(me, "signal");
	
	// Set up object components
	ShhhNN(me, iNN);

	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	ShhhNN(me, iNN)
 *	
 *	Set parameters, making sure nothing bad happens.
 *
 ******************************************************************************************/

void ShhhNN(
	tWhite* me,
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
		me->nn		= (int)iNN;
		me->mask	= kULongMax << (t_uint32)iNN;
		me->offset	= (~me->mask) >> 1;
		}
	
	}


/******************************************************************************************
 *
 *	ShhhTattle(me)
 *
 *	Post state information
 *
 ******************************************************************************************/

void
ShhhTattle(
	tWhite* me)
	
	{
	
	post(	"%s state",
			kClassName);
	post(	"  NN factor is: %d (mask = %lx)",
			me->nn,
			me->mask);
	
	}


/******************************************************************************************
 *
 *	ShhhAssist
 *	ShhhInfo(me)
 *
 *	Fairly generic Assist/Info methods
 *
 ******************************************************************************************/

void ShhhAssist(tWhite* me, void* box, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexInNN, strIndexOutWhite, oCStr);
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

void ShhhInfo(tWhite* me)
	{
	#pragma unused(me)
	
	LitterInfo(kClassName, NIL, NIL);
	}

#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	ShhhDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/
/*
void
ShhhDSP(
	tWhite*		me,
	t_signal**	ioDSPVectors,
	short*		connectCounts)
	
	{
	#pragma unused(connectCounts)
	
	dsp_add(
		ShhhPerform, 3,
		me, (long) ioDSPVectors[outletWhite]->s_n, ioDSPVectors[outletWhite]->s_vec
		);
	
	}
*/
void ShhhDSP64(tWhite* me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags) {
    object_method(dsp64, gensym("dsp_add64"), me, ShhhPerform64, 0, NULL);
    
}
void ShhhPerform64(tWhite* me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
	
	{
	
	t_uint32			vecCounter;
	tSampleVector	outNoise;
	//tWhite*			me = (tWhite*) iParams[paramMe];
	
	if (me->coreObject.z_disabled) return;
	
	// Copy parameters into registers
        vecCounter	= (t_uint32)sampleframes;
	outNoise	= outs[0];
	
	// Do our stuff
	if (me->nn == 0)
			Taus88SigVector(outNoise, vecCounter);
	else	Taus88SigVectorMasked(outNoise, vecCounter, me->mask, me->offset);
	

	}
