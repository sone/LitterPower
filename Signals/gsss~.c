/*
	File:		gsss~.c

	Contains:	Gaussian noise.

	Written by:	Peter Castine

	Copyright:	© 2002 Peter Castine

	Change History (most recent first):

         <8>   26–4–2006    pc      Update for new LitterLib organization.
         <7>   23–3–2006    pc      Use Marsaglia-Bray Gaussian generator
         <6>     9–12–04    pc      Update to use inlet_getproxy()
         <5>     14–1–04    pc      Update for Windows.
         <4>    6–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <3>  30–12–2002    pc      Drop faux 'Vers' resource and related modifications. Add object
                                    version to GsssInfo().
         <2>  28–11–2002    pc      Tidy up after initial check in.
         <1>  28–11–2002    pc      Initial check in.
		30-Jan-2002:	First implementation.
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"	// Also #includes MaxUtils.h, ext.h
//#include "TrialPeriodUtils.h"
#include "RNGGauss.h"
#include "Taus88.h"


#ifdef RC_INVOKED
#define LPmuString	"mu"
#else
#define LPmuString	"�"
#endif

// Assistance strings
#define LPAssistIn1			"Signal/float (" LPmuString ": DC Offset)"
#define LPAssistIn2			"Signal/float (s: Scaling factor)"
#define LPAssistOut1		"Signal (Gaussian noise)"


#pragma mark • Constants

const char*	kClassName		= "lp.gsss~";			// Class name

const int	kMaxNN			= 31;

/*	// Indices for STR# resource
enum {
	strIndexInMu		= lpStrIndexLastStandard + 1,
	strIndexInStdDev,
	strIndexOutGauss,
	
	strIndexInLeft		= strIndexInMu,
	strIndexOutLeft		= strIndexOutGauss
	};*/

	// Inlets/Outlet MSP knows about
enum {
	inletMu			= 0,
	inletStdDev,
	
	outletNoise
	};

#pragma mark • Type Definitions



#pragma mark • Object Structure

typedef struct {
	t_pxobject		coreObject;
	
	double			mu,
					stdDev;
	} objGaussNoise;


#pragma mark • Global Variables



#pragma mark • Function Prototypes

	// Class message functions
void*	GsssNew(double, double);

	// Object message functions
static void GsssFloat(objGaussNoise*, double);
//static void GsssInt(objGaussNoise*, long);
static void GsssMu(objGaussNoise*, double);
static void GsssStdDev(objGaussNoise*, double);
static void GsssTattle(objGaussNoise*);
static void	GsssAssist(objGaussNoise*, void* , long , long , char*);
static void	GsssInfo(objGaussNoise*);

	// MSP Messages
//static void	GsssDSP(objGaussNoise*, t_signal**, short*);
//static int*	GsssPerformStat(int*);
//static int*	GsssPerformDyn(int*);
void GsssDSP64(objGaussNoise*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void GsssPerformStat64(objGaussNoise*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void GsssPerformDyn64(objGaussNoise*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


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
	c = class_new(kClassName,				// Pointer to our class definition
			(method) GsssNew,			// Instance creation function
			(method) dsp_free,			// Default deallocation function
			sizeof(objGaussNoise),		// Class object size
			NIL,						// No menu function
			A_DEFFLOAT,					// Optional arguments:	1. Mu
			A_DEFFLOAT,					//						2. Std Var.
			0);	
	
	class_dspinit(c);

	// Messages
	class_addmethod(c,(method) GsssFloat, "float", A_FLOAT, 0);
	class_addmethod(c,(method) GsssTattle,	"dblclick",	A_CANT, 0);
	class_addmethod(c,(method) GsssTattle,	"tattle",	A_NOTHING);
	class_addmethod(c,(method) GsssAssist,	"assist",	A_CANT, 0);
	class_addmethod(c,(method) GsssInfo,		"info",		A_CANT, 0);
	
	// MSP-Level messages
	class_addmethod(c,(method) GsssDSP64, "dsp64", A_CANT, 0);

	//Initialize Litter Library
	//LitterInit(kClassName, 0);
	Taus88Init();
        class_register(CLASS_BOX, c);
        gObjectClass = c;
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;
	
	}



#pragma mark -
#pragma mark • Class Message Handlers

/******************************************************************************************
 *
 *	GsssNew(iMu, iStdDev, iNN)
 *	
 ******************************************************************************************/

void*
GsssNew(
	double	iMu,
	double	iStdDev)
	
	{
	objGaussNoise*		me	= NIL;
	
	// Take intialization parameters as they come. An undefined StdDev is tweaked in the
	// GsssStdDev method

	// Let Max/MSP allocate us, our inlets, and outlets.
	me = object_alloc(gObjectClass);
		if (me == NIL) goto punt;
		
	dsp_setup(&(me->coreObject), 2);
	
	outlet_new(me, "signal");
	
	// Set up object components
	GsssMu(me, iMu);
	GsssStdDev(me, iStdDev);

	// All done
punt:
	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	GsssNN(me, iNN)
 *	
 *	Set parameters, making sure nothing bad happens.
 *
 ******************************************************************************************/

	void GsssMu(objGaussNoise* me, double iMu)
		{ me->mu = iMu; }
		
	void GsssStdDev(objGaussNoise* me, double iStdDev)
		{
		const double	kDefStdDev = 0.4082482905;		// sqrt(1/6)
		me->stdDev = (iStdDev == 0.0) ? kDefStdDev : fabs(iStdDev);
		}
	

void GsssFloat(
	objGaussNoise*	me,
	double			iVal)
	
	{
	
	switch ( ObjectGetInlet((t_object*) me, me->coreObject.z_in) ) {
		case inletMu:
			GsssMu(me, iVal);
			break;
		case inletStdDev:
			GsssStdDev(me, iVal);
			break;
		default:
			// It can't happen here...
			break;
		}
		
	}


/******************************************************************************************
 *
 *	GsssTattle(me)
 *
 *	Post state information
 *
 ******************************************************************************************/

void
GsssTattle(
	objGaussNoise* me)
	
	{
	
	post("%s state",
			kClassName);
	post("  Mu (DC Offset) is: %f; Standard Variation is %f",
			me->mu,
			me->stdDev);
	
	}


/******************************************************************************************
 *
 *	GsssAssist()
 *	GsssInfo()
 *
 *	Fairly generic Assist/Info methods
 *
 ******************************************************************************************/

void GsssAssist(objGaussNoise* me, void* box, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexInLeft, strIndexOutLeft, oCStr);
        
        if (iDir == ASSIST_INLET) {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistIn1); break;
                case 1: sprintf (oCStr, LPAssistIn2); break;
            }
        }
        else {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistOut1); break;
            }
            
        }

	}

void GsssInfo(objGaussNoise* me)
	{ LitterInfo(kClassName, &me->coreObject.z_ob, (method) GsssTattle); }



#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	GsssDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/
/*
void
GsssDSP(
	objGaussNoise*	me,
	t_signal**		ioDSPVectors,
	short*			iConnectCounts)
	
	{
	
	// Can a signal vector have an odd size?
	// Not very likely. Since the creation of MSP (and presumably all of its forerunners),
	// Vector size has always been a positive power of two. All FFT objects will rely on
	// this, and David Zicarelli has indicated that an odd vector size is extremely unlikely.
	// So, I won't bloat the code with a check here. But we may regret this some day.
	
	if (iConnectCounts[inletMu] == 0 && iConnectCounts[inletStdDev] == 0)
			dsp_add(
				GsssPerformStat, 3,
				me, (long) ioDSPVectors[outletNoise]->s_n,
				ioDSPVectors[outletNoise]->s_vec
				);
		
	else	dsp_add(
				GsssPerformDyn, 5,
				me, (long) ioDSPVectors[inletMu]->s_n,
				iConnectCounts[inletMu] > 0 ? ioDSPVectors[inletMu]->s_vec : NIL,
				iConnectCounts[inletStdDev] > 0 ? ioDSPVectors[inletStdDev]->s_vec : NIL,
				ioDSPVectors[outletNoise]->s_vec
				);
	
	}
*/

void GsssDSP64(objGaussNoise *me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    if (count[inletMu] == 0 && count[inletStdDev] == 0)
        object_method(dsp64, gensym("dsp_add64"), me, GsssPerformStat64, 0, NULL);

    else
        object_method(dsp64, gensym("dsp_add64"), me, GsssPerformDyn64, 0, NULL);

}



/******************************************************************************************
 *
 *	GsssPerformStat(iParams)
 *	GsssPerformDyn(iParams)
 *
 *	GsssPerformStat is used when neither Gauss parameter has a signal connected.
 *	GsssPerformDyn is used if either parameters is controlled by a signal.
 *	
 *	Parameter block for GsssPerformStat contains 4 values:
 *		- Address of this function
 *		- The performing lp.gsss~ object
 *		- Vector size
 *		- output signal
 *
 *	Parameter block for GsssPerformStat contains 6 values:
 *		- Address of this function
 *		- The performing lp.gsss~ object
 *		- Vector size
 *		- Signal vector for mu (may be NIL)
 *		- Signal vector for StdDev (may be NIL)
 *		- output signal
 *
 ******************************************************************************************/

void GsssPerformStat64(objGaussNoise *me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    double			mu,
    stdDev;
    long			vecCounter;
    tSampleVector	outNoise;
    
    if (me->coreObject.z_disabled) return;
    
    // Copy parameters into registers
    mu			= me->mu;
    stdDev		= me->stdDev;
    vecCounter	= sampleframes;
    outNoise	= (tSampleVector) outs[0];
    
    // Do our stuff
    if (mu == 0.0 && stdDev == 1.0)
        do { *outNoise++ = NormalKRTaus88(NIL); } while (--vecCounter > 0);
    else do { *outNoise++ = stdDev * NormalKRTaus88(NIL) + mu; } while (--vecCounter > 0);
    

}


void GsssPerformDyn64(objGaussNoise *me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    enum {
        paramInMu = 0,
        paramInStdDev
    };
    
    long			vecCounter;
    tSampleVector	muSig,
    sdSig,
    outNoise;
    
    if (me->coreObject.z_disabled) return;
    
    // Copy parameters into registers
    vecCounter	= sampleframes;
    muSig		= ins[paramInMu];
    sdSig		= ins[paramInStdDev];
    outNoise	= outs[0];
    
    // ASSERT: vecCount is even.
    if (muSig && sdSig) do {
        *outNoise++ = *sdSig++ * NormalKRTaus88(NIL) + *muSig++;
    } while (--vecCounter > 0);
    
    else if (muSig) {
        // No signal for Std. Dev.
        double stdDev = me->stdDev;
        do	{
            *outNoise++ = stdDev * NormalKRTaus88(NIL) + *muSig++;
        } while (--vecCounter > 0);
    }
    else {
        // ASSERT: no signal for µ, but there must be one for Std.Dev.
        double mu = me->mu;
        do	{
            *outNoise++ = *sdSig++ * NormalKRTaus88(NIL) + mu;
        } while (--vecCounter > 0);
    }
    
}


/*
int*
GsssPerformStat(
	int iParams[])
	
	{
	enum {
		paramFuncAddress	= 0,
		paramMe,
		paramVectorSize,
		paramOut,
		
		paramNextLink
		};
	
	double			mu,
					stdDev;
	long			vecCounter;
	tSampleVector	outNoise;
	objGaussNoise*	me = (objGaussNoise*) iParams[paramMe];
	
	if (me->coreObject.z_disabled) goto exit;
	
	// Copy parameters into registers
	mu			= me->mu;
	stdDev		= me->stdDev;
	vecCounter	= (long) iParams[paramVectorSize];
	outNoise	= (tSampleVector) iParams[paramOut];
	
	// Do our stuff
	if (mu == 0.0 && stdDev == 1.0)
		 do { *outNoise++ = NormalKRTaus88(NIL); } while (--vecCounter > 0);
	else do { *outNoise++ = stdDev * NormalKRTaus88(NIL) + mu; } while (--vecCounter > 0);
		
exit:
	return iParams + paramNextLink;
	}

int*
GsssPerformDyn(
	int iParams[])
	
	{
	enum {
		paramFuncAddress	= 0,
		paramMe,
		paramVectorSize,
		paramInMu,
		paramInStdDev,
		paramOut,
		
		paramNextLink
		};
		
	long			vecCounter;
	tSampleVector	muSig,
					sdSig,
					outNoise;
	objGaussNoise*	me = (objGaussNoise*) iParams[paramMe];
	
	if (me->coreObject.z_disabled) goto exit;
	
	// Copy parameters into registers
	vecCounter	= (long) iParams[paramVectorSize];
	muSig		= (tSampleVector) iParams[paramInMu];
	sdSig		= (tSampleVector) iParams[paramInStdDev];
	outNoise	= (tSampleVector) iParams[paramOut];
	
	// ASSERT: vecCount is even.
	if (muSig && sdSig) do {
		*outNoise++ = *sdSig++ * NormalKRTaus88(NIL) + *muSig++;
		} while (--vecCounter > 0);
		
	else if (muSig) {
		// No signal for Std. Dev.
		double stdDev = me->stdDev;
		do	{
			*outNoise++ = stdDev * NormalKRTaus88(NIL) + *muSig++;
			} while (--vecCounter > 0);
		}
	else {
		// ASSERT: no signal for µ, but there must be one for Std.Dev.
		double mu = me->mu;
		do	{
			*outNoise++ = *sdSig++ * NormalKRTaus88(NIL) + mu;
			} while (--vecCounter > 0);
		}
	
exit:
	return iParams + paramNextLink;
	
	}
*/
