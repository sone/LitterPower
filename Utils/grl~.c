/*
	File:		grl~.c

	Contains:	Performs unwrapping, as in phase unwrapping of spectral signals. The
				default parameters are set up for phase unwrapping, but the object can
				conceivably be used for other purposes

	Written by:	Peter Castine

	Copyright:	© 2000-01 Peter Castine

	Change History (most recent first):

        <10>   23–3–2006    pc      Shouldn't be using getbytes() in our New() method.
         <9>     11–1–04    pc      Update for modified LitterInit()
         <8>      8–1–04    pc      Update for Windows.
         <7>    6–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <6>   30–3–2003    pc      Moved floatPtr typedef to MaxUtils.h
         <5>   22–3–2003    pc      Moved dsp_free() to start of free routine (new recommendation
                                    from C74)
         <4>  30–12–2002    pc      Add object version to GrlInfo()
         <3>  30–12–2002    pc      Use 'STR#' resource instead of faux 'Vers' resource for storing
                                    version information used at run-time.
         <2>  28–11–2002    pc      Tidy up after initial check in.
         <1>  28–11–2002    pc      Initial check in.
		24-Jun-2001:	Adapted for inclusion in Litter Power Package
		6-Dec-2000:		First implementation
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"
//#include "TrialPeriodUtils.h"


#pragma mark • Global Constants

const char*	kClassName		= "lp.grl~";			// Class name

#ifdef RC_INVOKED
#define piSymbol	"pi"
#else
#define piSymbol	"�"
#endif
#define LPAssistIn1			"Signal. Float or " piSymbol " sets maximum step between samples."
#define LPAssistOut1		"Unwrapped Signal"


/*
	// Indices for STR# resource
enum {
	strIndexInSig	= lpStrIndexLastStandard + 1,
	strIndexOutSig
	};

	// Indices for MSP Inlets and Outlets (don't count plain-vanilla inlets or outlets).
enum {
	inletSigIn		= 0,
	outletSigOut
	};
*/

#pragma mark • Object Structure

typedef struct {
	t_pxobject	coreObject;
	double		maxStep,
				prevSamp;			// Only used if histSize == 1;
	// Circular buffer for maintaining a history of elements
	unsigned	histSize,
				curElem;
	//floatPtr	buffer;
    double      *buffer;
	} tRanger;


#pragma mark • Global Variables

#pragma mark • Function Prototypes

static void*	GrlNew(long, double);
static void		GrlFree(tRanger*);
//static void		GrlDSP(tRanger*, t_signal**, short*);
//static int*		GrlPerformSimple(int*);
//static int*		GrlPerformHist(int*);
void GrlDSP64(tRanger*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void GrlPerformSimple64(tRanger*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);
void GrlPerformHist64(tRanger*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


	// Various Max messages
static void		GrlAssist(tRanger*, void* , long , long , char*);
static void		GrlInfo(tRanger*);

static void		GrlFloat(tRanger*, double);
static void		GrlPi(tRanger*, long);
static void		GrlClear(tRanger*);


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
	c = class_new(kClassName,				// Pointer to our class definition
			(method) GrlNew,			// Instance creation function
			(method) GrlFree,		// Custom deallocation function
			(short) sizeof(tRanger),	// Class object size
			NIL,						// No menu function
			A_DEFLONG,					// History size. Default 0 interpreted as 1
			A_DEFFLOAT,					// Maximum step between consecutive samples.
										//	default (i.e. 0) interpreted as pi.
			0);		
	
	// Max/MSP class initialization mantra, including registering all aliases
	class_dspinit(c);
	
	// Parameter messages
	class_addmethod(c,(method) GrlFloat,"float", A_FLOAT, 0);
	class_addmethod(c,(method) GrlPi,	"pi",		A_DEFLONG, 0);
	class_addmethod(c,(method) GrlPi,	"π",		A_DEFLONG, 0);
	class_addmethod(c,(method) GrlClear,"clear",	A_NOTHING, 0);
	
	// Fairly standard Max messages
	class_addmethod(c,(method) GrlAssist,"assist",	A_CANT, 0);
	class_addmethod(c,(method) GrlInfo,  "info",		A_CANT, 0);
	
	// MSP-Level messages
	class_addmethod(c,(method) GrlDSP64,	"dsp64",		A_CANT, 0);

	// Initialize Litter Library
	//LitterInit(kClassName, 0);
        class_register(CLASS_BOX, c);
        gObjectClass = c;
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;

	}

#pragma mark -
#pragma mark • Class Methods

/******************************************************************************************
 *
 *	GrlNew(iHistSize, iMaxStep)
 *
 ******************************************************************************************/

void*
GrlNew(
	long	iHistSize,
	double	iMaxStep)
	
	{
	const unsigned	kMinHistSize	= 1,
					kMaxHistSize	= 2048;

	tRanger*	me			= NIL;
	//floatPtr	histBuf		= NIL;
    double      *histBuf		= NIL;
	
	// First things first: allocate memory for our circular buffer
	// BTW: Make sure we have a valid argument for history size.
	if (iHistSize < kMinHistSize)
		iHistSize = kMinHistSize;
	else if (kMaxHistSize < iHistSize)
		iHistSize = kMaxHistSize;
	
	if (iHistSize > 1) {
		//histBuf = (float*) NewPtr(iHistSize * sizeof(float));
        histBuf = (double*) sysmem_newptr(iHistSize * sizeof(double));
		if (histBuf == NIL) goto punt;
		}
	else iHistSize = 1;		// Default value, also used if invalid negative argument given.
	
	// So far, so good...
	// Let Max/MSP allocate us and give us inlets;
	me = object_alloc(gObjectClass);
	dsp_setup(&(me->coreObject), 1);
	
	// And one outlet
	outlet_new(me, "signal");			
	
	me->maxStep		= (iMaxStep == 0.0) ? kPi : iMaxStep;
	me->prevSamp	= 0.0;				// Not needed if iHistSize > 1, but it doesn't hurt to
										// to initialize this space.
	me->histSize	= iHistSize;
	me->buffer		= histBuf;
	me->curElem		= 0;
	
	if (histBuf != NIL) {
		// ASSERT: iHistSize > 1
		do {
			*histBuf++ = 0.0;
			} while (--iHistSize > 0);
		}
	// NB:	iHistSize and histBuf are now invalid.
	//		But it looks as if they're not needed anymore.
	
punt:
	return me;
	}


/******************************************************************************************
 *
 *	GrlFree(me)
 *
 ******************************************************************************************/

void
GrlFree(
	tRanger* me)
	
	{
	
	// First call dsp_free() to clean up DSP chain
	dsp_free(&(me->coreObject));
	
	if (me->buffer != NIL)
		sysmem_freeptr(me->buffer);
	
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	GrlFloat(me, iValue)
 *	GrlPi(me, iFactor)
 *	GrlClear(me)
 *
 ******************************************************************************************/

void GrlFloat(tRanger*	me, double iVal)
	{ me->maxStep = iVal; }

void GrlPi(tRanger* me, long iFactor)
	{ me->maxStep = (iFactor <= 0) ? kPi : iFactor * kPi; }

void GrlClear(tRanger* me)
	{
	if (me->buffer != NIL) {
		// ASSERT: me->histSize > 1
		unsigned	counter = me->histSize;
		double      *p		= me->buffer;
		do {
			*p++ = 0.0;
			} while (--counter > 0);
		me->curElem = 0.0;
		}
	else me->prevSamp = 0.0;
	}
		
/******************************************************************************************
 *
 *	GrlAssist
 *	GrlInfo(me)
 *
 *	Fairly generic Assist/Info methods.
 *	
 *	We don't use a lot of the parameters.
 *
 ******************************************************************************************/

void GrlAssist(tRanger* me, void* box, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexInSig, strIndexOutSig, oCStr);
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

void GrlInfo(tRanger* me)
	{
	#pragma unused(me)
	
	LitterInfo(kClassName, NIL, NIL);
	}

	


#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	GrlDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/
/*
void
GrlDSP(
	tRanger*	me,
	t_signal**	ioDSPVectors,
	short		connectCounts[])		// David sez we don't need this,
										// cf. Chap 3 of the SDK documentation.
	
	{
	#pragma unused(connectCounts)
	
	if (me->histSize > 1)
			dsp_add(
				GrlPerformHist, 4,
				me, (long) ioDSPVectors[inletSigIn]->s_n,
				ioDSPVectors[inletSigIn]->s_vec,
				ioDSPVectors[outletSigOut]->s_vec
				);
	
	else	dsp_add(
				GrlPerformSimple, 4,
				me, (long) ioDSPVectors[inletSigIn]->s_n,
				ioDSPVectors[inletSigIn]->s_vec,
				ioDSPVectors[outletSigOut]->s_vec
				);
	
	}*/

void GrlDSP64(tRanger* me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    if (me->histSize > 1)
        object_method(dsp64, gensym("dsp_add64"), me, GrlPerformHist64, 0, NULL);
    else
        object_method(dsp64, gensym("dsp_add64"), me, GrlPerformSimple64, 0, NULL);
    
}


void GrlPerformSimple64(tRanger *me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    long			vecCounter;
    tSampleVector	inSig, outSig;
    double			maxStep, maxStep2, prevSamp;
    
    if (me->coreObject.z_disabled) return;
    
    vecCounter	= sampleframes;
    inSig		= ins[0];
    outSig		= outs[0];
    
    maxStep		= me->maxStep;
    maxStep2	= maxStep + maxStep;
    prevSamp	= me->prevSamp;
    
    if (maxStep > 0.0) do {
        // Unwrap
        t_sample	curSamp = *inSig++,
        diff	= fmod(curSamp - prevSamp, maxStep2);
        if (diff > maxStep) {
            diff -= maxStep2;
        }
        else if (diff < -maxStep) {
            diff += maxStep2;
        }
        *outSig++ = prevSamp +=  diff;
    } while (--vecCounter > 0);
    
    else if (maxStep == 0.0) do {
        *outSig++ = prevSamp;
    } while (--vecCounter > 0);
    
    else if (inSig != outSig) {
        // me->min > me->max, so we do no unwrapping, just copy samples
        // Note that we're so lazy we don't even do that much if inSig == outSig.
        do {
            *outSig++ = *inSig++;
        } while (--vecCounter > 0);
        
        // We need an updated value for prevSig
        prevSamp = outSig[-1];
    }
    
    else {
        // We just need an updated value for prevSig
        prevSamp = outSig[vecCounter];
    }
    
    // Whether or not we used me->prevSamp in this run, we must update it. It might
    // be used in the next run.
    me->prevSamp = prevSamp;
    
}

void GrlPerformHist64(tRanger *me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    long			vecCounter;
    tSampleVector	inSig, outSig;
    t_double		maxStep, maxStep2;
    double*			histPtr;
    unsigned		bufCounter;
    
    if (me->coreObject.z_disabled) return;
    
    vecCounter	= sampleframes;
    inSig		= ins[0];
    outSig		= outs[0];
    
    maxStep		= me->maxStep;
    maxStep2	= maxStep + maxStep;
    histPtr		= me->buffer + me->curElem;
    bufCounter	= me->histSize - me->curElem;
    
    // Now that I have local copies of the descriptors for my circular buffer,
    // I update the values stored in the unwrap~ object. Best to do this now
    // before vecCounter is invalidated from my abusing it as loop counter.
    me->curElem = (me->curElem + vecCounter) % me->histSize;
    
    // What happens next depends upon the current value of maxStep
    if (maxStep > 0.0) do {
        // Unwrap
        t_sample	curSamp		= *inSig++,
        prevSamp	= *histPtr,
        diff		= fmod(curSamp - prevSamp, maxStep2);
        
        if (diff > maxStep) {
            diff -= maxStep2;
        }
        else if (diff < -maxStep) {
            diff += maxStep2;
        }
        *histPtr++ = *outSig++ = prevSamp +=  diff;
        if (--bufCounter == 0) {
            bufCounter	= me->histSize;
            histPtr		= me->buffer;
        }
    } while (--vecCounter > 0);
    
    else if (maxStep2 == 0.0) do {
        *outSig++ = *histPtr++;
        if (--bufCounter == 0) {
            bufCounter	= me->histSize;
            histPtr		= me->buffer;
        }
    } while (--vecCounter > 0);
    
    else if (inSig != outSig) do {
        // me->min > me->max, so we do no unwrapping, just copy samples
        // Note that we're so lazy we don't even do that much if inSig == outSig.
        *histPtr++ = *outSig++ = *inSig++;
        if (--bufCounter == 0) {
            bufCounter	= me->histSize;
            histPtr		= me->buffer;
        }
    } while (--vecCounter > 0);
    
    else {
        // Well, we don't need to copy MSP signal vectors, but we *do* need to maintain
        // our own history
        do {
            *histPtr++ = *inSig++;
            if (--bufCounter == 0) {
                bufCounter	= me->histSize;
                histPtr		= me->buffer;
            }
        } while (--vecCounter > 0);
    }
    
}

/******************************************************************************************
 *
 *	PerformRanger(iParams)
 *
 *	Parameter block contains 5 values:
 *		- Address of this function
 *		- The performing Ranger object
 *		- Current signal vector size
 *		- Input signal
 *		- Output signal
 *
 ******************************************************************************************/
/*
	// Used by both GrlPerformSimple() and GrlPerformHist()
enum {
	paramFuncAddress	= 0,
	paramMe,
	paramVectorSize,
	paramInput,
	paramOutput,
	
	paramNextLink
	};

int*
GrlPerformSimple(
	int* iParams)
	
	{
	long			vecCounter;
	tSampleVector	inSig,
					outSig;
	float			maxStep,
					maxStep2,
					prevSamp;
	tRanger*		me = (tRanger*) iParams[paramMe];
	
	if (me->coreObject.z_disabled) goto exit;
	
	vecCounter	= (long) iParams[paramVectorSize];
	inSig		= (tSampleVector) iParams[paramInput];
	outSig		= (tSampleVector) iParams[paramOutput];
	
	maxStep		= me->maxStep;
	maxStep2	= maxStep + maxStep;
	prevSamp	= me->prevSamp;
	
	if (maxStep > 0.0) do {
		// Unwrap
		t_sample	curSamp = *inSig++,
					diff	= fmod(curSamp - prevSamp, maxStep2);
		if (diff > maxStep) {
			diff -= maxStep2;
			}
		else if (diff < -maxStep) {
			diff += maxStep2;
			}
		*outSig++ = prevSamp +=  diff;
		} while (--vecCounter > 0);
	
	else if (maxStep == 0.0) do {
		*outSig++ = prevSamp;
		} while (--vecCounter > 0);
	
	else if (inSig != outSig) {
		// me->min > me->max, so we do no unwrapping, just copy samples
		// Note that we're so lazy we don't even do that much if inSig == outSig.
		do {
			*outSig++ = *inSig++;
			} while (--vecCounter > 0);
		
		// We need an updated value for prevSig
		prevSamp = outSig[-1];
		}
	
	else {
		// We just need an updated value for prevSig
		prevSamp = outSig[vecCounter];
		}
		
	// Whether or not we used me->prevSamp in this run, we must update it. It might
	// be used in the next run.
	me->prevSamp = prevSamp;
	
exit:
	return iParams + paramNextLink;
	}


int*
GrlPerformHist(
	int* iParams)
	
	{
	long			vecCounter;
	tSampleVector	inSig,
					outSig;
	float_t			maxStep,
					maxStep2;
	float*			histPtr;
	unsigned		bufCounter;
	tRanger*		me = (tRanger*) iParams[paramMe];
	
	if (me->coreObject.z_disabled) goto exit;
	
	vecCounter	= (long) iParams[paramVectorSize];
	inSig		= (tSampleVector) iParams[paramInput];
	outSig		= (tSampleVector) iParams[paramOutput];
	
	maxStep		= me->maxStep;
	maxStep2	= maxStep + maxStep;
	histPtr		= me->buffer + me->curElem;
	bufCounter	= me->histSize - me->curElem;
	
	// Now that I have local copies of the descriptors for my circular buffer,
	// I update the values stored in the unwrap~ object. Best to do this now
	// before vecCounter is invalidated from my abusing it as loop counter.
	me->curElem = (me->curElem + vecCounter) % me->histSize;
	
	// What happens next depends upon the current value of maxStep
	if (maxStep > 0.0) do {
		// Unwrap
		t_sample	curSamp		= *inSig++,
					prevSamp	= *histPtr,
					diff		= fmod(curSamp - prevSamp, maxStep2);
		
		if (diff > maxStep) {
			diff -= maxStep2;
			}
		else if (diff < -maxStep) {
			diff += maxStep2;
			}
		*histPtr++ = *outSig++ = prevSamp +=  diff;
		if (--bufCounter == 0) {
			bufCounter	= me->histSize;
			histPtr		= me->buffer;
			}
		} while (--vecCounter > 0);
	
	else if (maxStep2 == 0.0) do {
		*outSig++ = *histPtr++;
		if (--bufCounter == 0) {
			bufCounter	= me->histSize;
			histPtr		= me->buffer;
			}
		} while (--vecCounter > 0);
	
	else if (inSig != outSig) do {
		// me->min > me->max, so we do no unwrapping, just copy samples
		// Note that we're so lazy we don't even do that much if inSig == outSig.
		*histPtr++ = *outSig++ = *inSig++;
		if (--bufCounter == 0) {
			bufCounter	= me->histSize;
			histPtr		= me->buffer;
			}
		} while (--vecCounter > 0);
	
	else {
		// Well, we don't need to copy MSP signal vectors, but we *do* need to maintain
		// our own history
		do {
			*histPtr++ = *inSig++;
			if (--bufCounter == 0) {
				bufCounter	= me->histSize;
				histPtr		= me->buffer;
				}
			} while (--vecCounter > 0);
		}
	
exit:
	return iParams + paramNextLink;
	}
*/
