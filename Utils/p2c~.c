/*
	File:		p2c~.c

	Contains:	Convert polar to Cartesian representation of signals.

	Written by:	Peter Castine

	Copyright:	© 2000-01 Peter Castine

	Change History (most recent first):

         <7>     11–1–04    pc      Update for modified LitterInit()
         <6>      8–1–04    pc      Update for Windows.
         <5>    6–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <4>  30–12–2002    pc      Add object version to P2CInfo()
         <3>  30–12–2002    pc      Use 'STR#' resource instead of faux 'Vers' resource for storing
                                    version information used at run-time.
         <2>  28–11–2002    pc      Tidy up after initial check in.
         <1>  28–11–2002    pc      Initial check in.
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include <stdlib.h>		// For rand(), RAND_MAX
#include <math.h>		// For pow()

#include "LitterLib.h"
//#include "TrialPeriodUtils.h"

// Assistance strings
#define LPAssistIn1         "Signal (Amplitude)"
#define LPAssistIn2         "Signal (Phase)"
#define LPAssistOut1        "Signal (Real component)"
#define LPAssistOut2        "Signal (Imaginary component)"

#pragma mark • Global Constants

const char*	kClassName		= "lp.p2c~";				// Class name

	// Indices for STR# resource
enum {
	strIndexInAmp		= lpStrIndexLastStandard + 1,
	strIndexInPhase,
	
	strIndexOutReal,
	strIndexOutImag
	};

	// Indices for Inlets and Outlets. Users count starting at 1, we count from 0
	// 
enum {
	inletAmp		= 0,
	inletPhase,
	
	outletReal,
	outletImag
	};


#pragma mark • Type Definitions

#pragma mark • Object Structure

typedef struct {
	t_pxobject	coreObject;
	} tConverter;


#pragma mark • Global Variables

#pragma mark • Function Prototypes

static void*	P2CNew(void);
//static void	P2CDSP(tConverter*, t_signal**, short*);
//static int*	P2CPerform(int*);
void P2CDSP64(tConverter*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void P2CPerform64(tConverter*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

	// Various Max messages
static void	P2CAssist(tConverter*, void* , long , long , char*);
static void	P2CInfo(tConverter*);


#pragma mark -
/*****************************  I M P L E M E N T A T I O N  ******************************/

/******************************************************************************************
 *
 *	main()
 *	
 *	Standard MSP External Object Entry Point Function
 *	
 ******************************************************************************************/

int C74_EXPORT main(void)
	
	{
	//LITTER_CHECKTIMEOUT(kClassName);
        t_class *c;
	
	// Standard Max setup() call
	c = class_new(kClassName,					// Pointer to our class definition
			(method) P2CNew,				// Instance creation function
			(method) dsp_free,				// Default deallocation function
			(short) sizeof(tConverter),		// Class object size
			NIL,							// No menu function
			A_NOTHING);						// No arguments
	
	// Max/MSP class initialization mantra, including registering all aliases
	class_dspinit(c);
	
	// Fairly standard Max messages
	class_addmethod(c,(method) P2CAssist,	"assist",	A_CANT, 0);
	class_addmethod(c,(method) P2CInfo,		"info",		A_CANT, 0);
	
	// MSP-Level messages
	class_addmethod(c,(method) P2CDSP64,    "dsp64", A_CANT, 0);
	
	// Initialize Litter Library
	//LitterInit(kClassName, 0);
        class_register(CLASS_BOX, c);
        gObjectClass = c;
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;
	
	}

#pragma mark -
#pragma mark • Methods

/******************************************************************************************
 *
 *	P2CNew()
 *
 ******************************************************************************************/

void*
P2CNew(void)
	
	{
	tConverter*	me	= NIL;
	
	// Let Max/MSP allocate us and give us inlets;
	me = object_alloc(gObjectClass);
	dsp_setup(&(me->coreObject), 2);
	
	// And two outlets, set these up right-to-left
	outlet_new(me, "signal");			
	outlet_new(me, "signal");			
	
	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	P2CAssist
 *	P2CInfo(me)
 *
 ******************************************************************************************/

void P2CAssist(tConverter* me, void* box, long iDir, long iArgNum, char oCStr[])
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexInAmp, strIndexOutReal, oCDestStr);
        if (iDir == ASSIST_INLET) {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistIn1); break;
                case 1: sprintf (oCStr, LPAssistIn2); break;
            }
        }
        else {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistOut1); break;
                case 1: sprintf (oCStr, LPAssistOut2); break;
            }
        }
	}

void P2CInfo(tConverter* me)
	{
	#pragma unused(me)
	
	LitterInfo(kClassName, NIL, NIL);
	}

	

#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	P2CDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/
/*
void
P2CDSP(
	tConverter*	me,
	t_signal**	ioDSPVectors,
	short*		iConnectCounts)
	
	{
	
	dsp_add(
		P2CPerform, 6,
		me,
		(long) ioDSPVectors[inletAmp]->s_n,
		iConnectCounts[inletAmp] ? ioDSPVectors[inletAmp]->s_vec : NIL,
		iConnectCounts[inletPhase] ? ioDSPVectors[inletPhase]->s_vec : NIL,
		iConnectCounts[outletReal] ? ioDSPVectors[outletReal]->s_vec : NIL,
		iConnectCounts[outletImag] ? ioDSPVectors[outletImag]->s_vec : NIL
		);

	}*/


void P2CDSP64(tConverter *me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    object_method(dsp64, gensym("dsp_add64"), me, P2CPerform64, 0, NULL);

}



/******************************************************************************************
 *
 *	P2CPerform(iParams)
 *
 *	Parameter block contains 7 values:
 *		- Address of this function
 *		- The performing Converter object
 *		- Current signal vector size
 *		- Real signal
 *		- Imaginary signal
 *		- Amplitude signal (out)
 *		- Phase signal (out)
 *
 ******************************************************************************************/


void P2CPerform64(tConverter *me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    enum {
        paramAmp = 0,
        paramPhase,
        
        paramReal = 0,
        paramImag,

    };
    
    long            vecCounter;
    tSampleVector    ampSig,
    phaseSig,
    realSig,
    imagSig;
    
    if (me->coreObject.z_disabled) return;
    
    vecCounter    = sampleframes;
    ampSig        = ins[paramAmp];
    phaseSig      = ins[paramPhase];
    realSig        = outs[paramReal];
    imagSig        = outs[paramImag];
    /*
    do {
        double    amp        = ampSig    ? *ampSig++        : 0.0,
        phase    = phaseSig    ? *phaseSig++    : 0.0;
        
        if (realSig) *realSig++    = amp * sin(phase);
        if (imagSig) *imagSig++    = amp * cos(phase);
    } while (--vecCounter > 0);
     */
    do {
        double    amp       = *ampSig++;
        double    phase     = *phaseSig++;
        
        *realSig++    = amp * sin(phase);
        *imagSig++    = amp * cos(phase);
    } while (--vecCounter > 0);
}


/*
int*
P2CPerform(
	int* iParams)
	
	{
	enum {
		paramFuncAddress	= 0,
		paramMe,
		paramVectorSize,
		paramAmp,
		paramPhase,
		paramImag,
		paramReal,
		
		paramNextLink
		};
	
	long			vecCounter;
	tSampleVector	ampSig,
					phaseSig,
					realSig,
					imagSig;
	tConverter*		me = (tConverter*) iParams[paramMe];
	
	if (me->coreObject.z_disabled) goto exit;
	
	vecCounter	= (long) iParams[paramVectorSize];
	ampSig		= (tSampleVector) iParams[paramAmp];
	phaseSig	= (tSampleVector) iParams[paramPhase];
	realSig		= (tSampleVector) iParams[paramReal];
	imagSig		= (tSampleVector) iParams[paramImag];
	
	do {
		double	amp		= ampSig	? *ampSig++		: 0.0,
				phase	= phaseSig	? *phaseSig++	: 0.0;
				
		if (realSig) *realSig++	= amp * sin(phase);
		if (imagSig) *imagSig++	= amp * cos(phase);
		} while (--vecCounter > 0);
		
exit:
	return iParams + paramNextLink;
	}
*/
