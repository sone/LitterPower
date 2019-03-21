/*
	File:		gruyère~.c

	Contains:	1st order "uniform" Markov chains for waveform generation

	Written by:	Peter Castine

	Copyright:	© 2004 Peter Castine

	Change History (most recent first):

         <3>   26–4–2006    pc      Update for new LitterLib organization.
         <2>    10–12–04    pc      Fix parameters to error message in GruyereSetBuf()
         <1>     9–12–04    pc      first checked in.
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"									// Also #includes MaxUtils.h, ext.h
//#include "TrialPeriodUtils.h"
#include "Taus88.h"

// Assistance strings
#define LPAssistIn1			"Signal or number (frequency)"
#define LPAssistIn2			"Signal or number (epsilon: phase shift probability)"
#define LPAssistOut1		"Signal (Uniform Markov noise)"

#pragma mark • Constants

const char*			kClassName		= "lp.gruyere~";		// Class name

#ifdef __GNUC__
	// This is so lame
	#define kTableLen	512
#else
	const int kTableLen		= 512;
#endif			// __GNUC__

const unsigned long	kTableLenMod	= 0x000001ff,
					kMaxUnitThresh	= 0x07ffffff;			// kMaxUInt / kTableLen

/*	// Indices for STR# resource
enum {
		// Inlets
	strIndexInFreq			= lpStrIndexLastStandard + 1,
	strIndexInEpsilon,
	
		// Outlets
	strIndexOutNoise,
	
		// Offsets for assiststring()
	strIndexInLeft			= strIndexInFreq,
	strIndexOutLeft			= strIndexOutNoise
	};*/
	
	// Indices for tracking our proxies
enum {
	proxyFreq			= 0,
	proxyEpsilon,
	
	proxyCount
	};

	// ...and for inlets/outlets
enum {
	inletFreq				= 0,
	inletEpsilon,
	
	outletNoise
	};
	
	
#pragma mark • Type Definitions


#pragma mark • Object Structure

typedef struct {
	t_pxobject		coreObject;
	
	SymbolPtr		bufSym;
	tBufferPtr		buf;
    t_buffer_ref    *bufref;
    t_symbol        *bufname;
	
	unsigned long	bufOffset,			// User-specified offset into buffer table
					randThresh;			// (Taus88() > randThresh) ==> rand phase increment
										// else increment by 1
	
	double			freq,
					sr,
					stepFactor,
					phi,
					epsilon;
    
    short           connected[2];       // keep track of inlet connections
					
	} objGruyere;


#pragma mark • Global Variables

float	gCosine[kTableLen];
//t_sample	gCosine[kTableLen];

#pragma mark • Function Prototypes

	// Class message functions
void*	GruyereNew(SymbolPtr, short, t_atom[]);

	// Object message functions
static void GruyereInt(objGruyere*, long);
static void GruyereFloat(objGruyere*, double);
static void GruyereSetBuf(objGruyere*, t_symbol*, long);
static void GruyereTattle(objGruyere*);
static void GruyereDClick(objGruyere*);
static void	GruyereAssist(objGruyere*, void* , long , long , char*);
static void	GruyereInfo(objGruyere*);
//short GruyereSetBuf(objGruyere *x, t_symbol *s);
t_max_err GruyereNotify(objGruyere *me, t_symbol *s, t_symbol *msg, void *sender, void *data);

	// MSP Messages
//static void	GruyereDSP(objGruyere*, t_signal**, short*);
//static int*	GruyerePerform(int*);
void GruyereDSP64(objGruyere*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void GruyerePerform64(objGruyere*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);


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

	static void InitCosineTable()
		{
		const double kThetaIncr = k2pi / (double) kTableLen;
		
		int		i;
		double	theta;
		
		for (	i = 0, theta = 0.0;
				i < kTableLen;
				i += 1, theta += kThetaIncr) {
			gCosine[i] = cos(theta);
			}
		}

int C74_EXPORT main(void)
	
	{
	//LITTER_CHECKTIMEOUT(kClassName);
        t_class *c;
	
	// Get this out of the way
	InitCosineTable();
	
	// Standard Max/MSP initialization mantra
	c = class_new(kClassName,							// Pointer to our class definition
			(method) GruyereNew,					// Instance creation function
			(method) dsp_free,						// Default MSP deallocation function
			sizeof(objGruyere),						// Class object size
			NIL,									// No menu function
			A_GIMME,								// We parse the argument list ourselves
			0);
			
	class_dspinit(c);

	// Messages
	class_addmethod(c,(method) GruyereInt,      "int",      A_LONG, 0);
	class_addmethod(c,(method) GruyereFloat,    "float",    A_FLOAT, 0);
	class_addmethod(c,(method) GruyereSetBuf,	"set",		A_DEFSYM, A_DEFLONG, 0);
	class_addmethod(c,(method) GruyereDClick,	"dblclick",	A_CANT, 0);
	class_addmethod(c,(method) GruyereTattle,	"tattle",	A_NOTHING);
	class_addmethod(c,(method) GruyereAssist,	"assist",	A_CANT, 0);
	class_addmethod(c,(method) GruyereInfo,		"info",		A_CANT, 0);
    class_addmethod(c,(method) GruyereNotify,   "notify",   A_CANT, 0);
	
	// MSP-Level messages
	class_addmethod(c,(method) GruyereDSP64,	"dsp64",	A_CANT, 0);

	//Initialize Litter Library
	//LitterInit(kClassName, 0);
	Taus88Init();
        class_register(CLASS_BOX, c);
        gObjectClass = c;
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;
	
	}


#pragma mark -
#pragma mark • Utilities

/******************************************************************************************
 *
 *	SetSR(me, iSR)
 *	SetFreq(me, iFreq)
 *	
 ******************************************************************************************/

static void
SetSR(
	objGruyere*	me,
	double		iSR)
	
	{
	
	me->sr			= iSR;											// ASSERT: iSR > 0
	me->stepFactor	= (double) kTableLen * (me->freq / iSR);
	
	}


static void
SetFreq(
	objGruyere*	me,
	double		iFreq)
	
	{
        
	if (iFreq < 0.0)		// Actually, a negative frequency might make a subtle difference
		iFreq = - iFreq;	// to the sound, but the perform method is more efficient if
							// we don't have to check for negative increments.
							// And most people won't notice the difference.

	me->freq		= iFreq;
	me->stepFactor	= (double) kTableLen * (iFreq / me->sr);
	
	}

static void
SetEpsilon(
	objGruyere*	me,
	double		iEps)
	
	{
	
	// Clip to unit range
	if (iEps < 0.0)
		iEps = 0.0;
	else if (iEps > 1.0)
		iEps = 1.0;
	
	me->epsilon		= iEps;
	me->randThresh	= (1.0 - iEps) * (double) kULongMax;
	
	}

#pragma mark -
#pragma mark • Class Message Handlers

/******************************************************************************************
 *
 *	GruyereNew(iClassName, iArgC, iArgV)
 *
 *	GruyereNew() expects the arguments (in left-to-right order, all optional):
 *	
 *		- segment count							[Default: 2]
 *		- frequency								[Default: 440 Hz]
 *		- rate of pulse amplitude change		[Default: 0]
 *		- rate of pulse segment width change	[Default: 0]
 *
 *	If the rate of pulse/amplitude change is left at zero, interpolation will be off by
 *	default
 *	
 ******************************************************************************************/

void*
GruyereNew(
	SymbolPtr	iSym,							// Must be 'lp.gruyere~'
	short		iArgC,
	t_atom		iArgV[])
	
	{
	#pragma unused(iSym)
	
	SymbolPtr	bufSym		= NIL;				// Initialize to reasonable default values
	double		freq		= 0.0,
				epsilon		= 0.0;
	long		offset		= 0;
	Boolean		userFreq	= false,			// Track which numeric arguments have been
				userEpsilon	= false,			// specified
				userOffset	= false;
			
	
	objGruyere* me	= NIL;
	
	// Parse arguments
	while (iArgC-- > 0) {
		AtomPtr curArg = iArgV++;
		
		switch (curArg->a_type) {
		case A_SYM:
			if (bufSym == NIL)
				bufSym = curArg->a_w.w_sym;
				
			else error("%s: ignoring spurious symbol argument %s",
						kClassName,
						curArg->a_w.w_sym->s_name);
			break;
		
		case A_LONG:
		case A_FLOAT:
			if (!userFreq) {
				freq = AtomGetFloat(curArg);
				userFreq = true;
				}
			else if (!userEpsilon) {
				epsilon = AtomGetFloat(curArg);
				userEpsilon = true;
				}
			else if (!userOffset) {
				offset = AtomGetLong(curArg);
				userOffset = true;
				}
			else {
				error("%s: ignoring spurious argument", kClassName);
				postatom(curArg);
				}
			break;
		
		default:
			// This can't happen
			break;
			}
		
		iArgV++;
		}
	
	
	// Let Max/MSP allocate us, our inlets, and outlets.
	me = object_alloc(gObjectClass);
	dsp_setup(&(me->coreObject), 2);		// Signal inlets for frequency, etc.
	
	outlet_new(me, "signal");				// Outlet
	
	me->phi				= 0.0;
	me->sr				= sys_getsr();		// Get real sample rate in dsp method
    me->bufname         = bufSym;
	
	SetFreq(me, freq);						// Sets freq, stepFactor
	SetEpsilon(me, epsilon);				// Sets epsilon, randThresh
	GruyereSetBuf(me, bufSym, offset);			// Sets bufSym, buf, bufOffset
	
	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	GruyereFloat(me, iVal)
 *	GruyereInt(me, iVal)
 *
 *	GruyereInt() is only necessary because Max is to braindead to typecast ints to floats.
 *
 ******************************************************************************************/

void GruyereFloat(
	objGruyere*	me,
	double		iVal)
	
	{
	
	switch ( ObjectGetInlet((t_object*) me, me->coreObject.z_in) ) {
		case proxyEpsilon:	SetEpsilon(me, iVal);	break;
		default:			SetFreq(me, iVal);		break;
		}
	
	}

void GruyereInt(objGruyere* me, long iVal)
	{ GruyereFloat(me, (double) iVal); }


/******************************************************************************************
 *
 *	GruyereTattle(me)
 *	GruyereDClick(me)
 *	GruyereAssist
 *	GruyereInfo(me)
 *
 *	All fairly generic implementations
 *
 ******************************************************************************************/

void
GruyereSetBuf(
	objGruyere*	me,
	SymbolPtr	iSym,
	long		iOff)
	
	{
        if (iSym == gensym("") || iSym == NULL) {
            me->bufname     = NIL;
            me->bufOffset	= 0;
            return;
        }
        
        if (!me->bufref)
            me->bufref = buffer_ref_new((t_object*)me, iSym);
        else
            buffer_ref_set(me->bufref, iSym);
        
        if( !buffer_ref_exists(me->bufref) ) {
            me->bufref = NULL;
            object_error((t_object *)me, "buffer %s doesn't exist!", iSym->s_name);
            //return 0;
        }
        
        if (iSym == gensym(""))
            iSym = NIL;
        
        if (iOff < 0) {
            error("%s: invalid offset %ld", kClassName, iOff);
            iOff = 0;
        }
        
        me->bufname = iSym;
        me->bufOffset	= iOff;
        
	}
/*
void
GruyereSetBuf(
              objGruyere*	me,
              SymbolPtr	iSym,
              long		iOff)

{
    tBufferPtr buf;
    
    if (iSym == gensym(""))
        iSym = NIL;
    
    if (iOff < 0) {
        error("%s: invalid offset %ld", kClassName, iOff);
        iOff = 0;
    }
    
    if ( iSym != NIL
        && (buf = (tBufferPtr) iSym->s_thing) != NIL
        && ob_sym(buf) == gensym("buffer~") ) {
        
        me->bufSym		= iSym;
        me->buf			= buf;
        me->bufOffset	= iOff;
    }
    
    else {
        if (iSym != NIL)
            error("%s: no buffer~ %s", kClassName, iSym->s_name);
        
        me->bufSym		= NIL;
        me->buf			= NIL;
        me->bufOffset	= 0;
    }
    
    
}*/


/******************************************************************************************
 *
 *	GruyereTattle(me)
 *	GruyereDClick(me)
 *	GruyereAssist
 *	GruyereInfo(me)
 *
 *	All fairly generic implementations
 *
 ******************************************************************************************/

void
GruyereTattle(
	objGruyere* me)
	
	{
	const char	kOnStr[] = "on",
				kOffStr[] = "off";
	
	post("%s state:", kClassName);
	if (me->bufSym != NIL)
		 post("  Using wavetable in buffer %s", me->bufSym->s_name);
	else post("  Using default cosine wavetable (no buffer window available)");
	post("  frequency is %lf (stepFactor %lf at sample rate %lf)",
			me->freq, me->stepFactor, me->sr);
	post("  epsilon is %lf (randThresh is %lu)", me->epsilon, me->randThresh);
	
	}

void
GruyereDClick(
	objGruyere* me)
	
	{
	
	//if (me->buf == NIL)
		 GruyereTattle(me);
	//else //mess0((t_object*) me->buf, gensym("dblclick"));
        buffer_view(buffer_ref_getobject(me->bufref));
	
	}

void GruyereAssist(objGruyere* me, void* iBox, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, iBox)
	
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
                    //case 1: sprintf(s, "..."); break;
            }
            
        }

	}

void GruyereInfo(objGruyere* me)
	{ LitterInfo(kClassName, (tObjectPtr) me, (method) GruyereTattle); }


t_max_err GruyereNotify(objGruyere *me, t_symbol *s, t_symbol *msg, void *sender, void *data) {
    //post("registered name: %s", s->s_name);
    //post("notification/message: %s", msg->s_name);
    //TODO: should i be waiting for "buffer modified" message, and continue after that??
    // seems to work ok, like that...
    return buffer_ref_notify(me->bufref, s, msg, sender, data);
}


#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	GruyereDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/
/*
void
GruyereDSP(
	objGruyere*	me,
	t_signal**	ioDSPVectors,
	short*		iConnectCounts)
	
	{
	double sr = ioDSPVectors[0]->s_sr;
	
	// Make sure we have the correct sampling rate
	// (This can change between calls to our dsp method)
	if (me->sr != sr)
		SetSR(me, sr);
	
	// Make sure any buffer~ references are up-to-date
	if (me->bufSym != NIL)
		GruyereSetBuf(me, me->bufSym, me->bufOffset);
	
	dsp_add(
		GruyerePerform, 5,
		me, (long) ioDSPVectors[inletFreq]->s_n,
		(iConnectCounts[inletFreq] > 0)		? ioDSPVectors[inletFreq]->s_vec : NIL,
		(iConnectCounts[inletEpsilon] > 0)	? ioDSPVectors[inletEpsilon]->s_vec : NIL,
		ioDSPVectors[outletNoise]->s_vec
		);
	
	}
*/

void GruyereDSP64(objGruyere *me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
    double sr = samplerate;
    
    me->connected[0] = count[0];
    me->connected[1] = count[1];
    
    // Make sure we have the correct sampling rate
    // (This can change between calls to our dsp method)
    if (me->sr != sr)
        SetSR(me, sr);
    
    // Make sure any buffer~ references are up-to-date
    if (me->bufSym != NIL)
        GruyereSetBuf(me, me->bufSym, me->bufOffset);
    
    object_method(dsp64, gensym("dsp_add64"), me, GruyerePerform64, 0, NULL);

    
}





/******************************************************************************************
 *
 *	GruyerePerform(iParams)
 *
 *	Parameter block for GruyerePerform contains 6 values:
 *		- Address of this function
 *		- The performing gruyère~ object
 *		- Vector size
 *		- Frequency signal
 *		- AmpRate signal
 *		- WidthRate signal
 *		- Output signal
 *
 ******************************************************************************************/

	// Does buffer.h in the version of the SDK we're using include the b_inuse field in the
	// definition of a t_buffer?
	// Set up the test to make it easy to change if necessary
#if defined(WIN_VERSION)
	#define __BUFFER_HAS_INUSE_FLAG__ 1
#elif defined(MAC_VERSION)					// Must be OS X (Max/MSP 4.2 or later)
	#define __BUFFER_HAS_INUSE_FLAG__ 0
#else										// Must be Classic Mac OS (Max/MSP 4.1)
	#define __BUFFER_HAS_INUSE_FLAG__ 0
#endif


void GruyerePerform64(objGruyere *me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    enum {
        paramFreq = 0,
        paramEpsilon,
        paramOut = 0
    };
    long			i,
    tLen,
    nChans;
    unsigned long	rThresh;
    double			x,
    phi,
    stepFactor;
    //tBufferPtr		buf;
    t_buffer_obj    *b;
    float           *tab;
    //tSampleVector	wTable,
    float           *wTable;
    tSampleVector   out;
    
#if __BUFFER_HAS_INUSE_FLAG__
    long			inuseState;
#endif
    
    if (me->coreObject.z_disabled) return;
    
    // Need to update control-rate parameters?
    if (me->connected[paramFreq]) {
        x = *(ins[paramFreq]);
        if (x != me->freq) SetFreq(me, x);
    }
    if (me->connected[paramEpsilon]) {
        x = *(ins[paramEpsilon]);
        if (x != me->epsilon) SetEpsilon(me, x);
    }
    
    // Copy other parameters and some object members into registers
    i			= sampleframes;
    out			= outs[0];
    stepFactor	= me->stepFactor;
    //buf			= me->buf;
    rThresh		= me->randThresh;
    phi			= me->phi;
    
    b = buffer_ref_getobject(me->bufref);
    tab = buffer_locksamples(b);
    
    if (tab == NULL)  {
        // No buffer~ specified, so use default cosine table
        wTable	= gCosine;
        tLen	= kTableLen;
        nChans	= 1;
    }
    
    else {
        long offset, frames;
        frames = buffer_getframecount(b);
        nChans = buffer_getchannelcount(b);
        offset = me->bufOffset;
        
        if ( offset < frames) {
            wTable	= tab + offset;
            tLen	= tab - offset;
            
            if (tLen > kTableLen)
                tLen = kTableLen;
            
 /*
#if __BUFFER_HAS_INUSE_FLAG__
            inuseState		= buf->b_inuse;
            buf->b_inuse	= true;
#endif
  */
        }
        
        else {
            // Either buffer or offset is invalid
            // Zero output vector and punt
            do { *out++ = 0.0; } while (--i > 0);
            return;
        }
    }
    
    if (stepFactor > tLen) stepFactor = fmod(stepFactor, tLen);
    
    // Do our stuff
    // We can do this a little faster with the default sine table or a single-channel table
    // of the same size
    if (nChans == 1 && tLen == kTableLen) do {
        unsigned long	rand = Taus88(NIL);
        double			samp1, samp2, frac;
        int				index;
        
        if (rand <= rThresh) {
            phi += stepFactor;
            if (phi >= (double) kTableLen) phi -= (double) kTableLen;
        }
        else {
            phi += stepFactor * (rand & kTableLenMod);
            if (phi >= tLen) phi = fmod(phi, (double) kTableLen);
        }
        
        index	= phi;							// Truncate to int
        frac	= phi - floor(phi);
        
        samp1 = wTable[index++];
        samp2 = (index < kTableLen) ? wTable[index] : wTable[0];
        
        *out++ = (1.0 - frac) * samp1 + frac * samp2;
    } while (--i > 0);
    
    else do {
        unsigned long	rand = Taus88(NIL);
        double			samp1, samp2, frac;
        int				index;
        
        if (rand <= rThresh) {
            phi += stepFactor;
            if (phi >= (double) kTableLen) phi -= (double) kTableLen;
        }
        else {
            phi += stepFactor * (rand % tLen);
            if (phi >= tLen) phi = fmod(phi, (double) kTableLen);
        }
        
        index	= phi;							// Truncate to int
        frac	= phi - floor(phi);
        
        samp1 = (index < tLen) ? wTable[index * nChans] : 0.0;
        index += 1;
        samp2 = (index < kTableLen)
        ? ( (index < tLen) ? wTable[index * nChans] : 0.0 )
        : wTable[0];
        
        *out++ = (1.0 - frac) * samp1 + frac * samp2;
    } while (--i > 0);
    
    buffer_unlocksamples(b);
    
    // Update object members
    me->phi	= phi;
  
    /*
#if __BUFFER_HAS_INUSE_FLAG__
    // Do we need to restore the state of buffer's b_inuse flag?
    // ASSERT: the following condition evaluates to true if and only if
    //				- buf was valid
    //				- we stored the previous value of buf->b_inuse
    if (wTable != gCosine)
        buf->b_inuse = inuseState;
#endif
     */

}
/*
int*
GruyerePerform(
	int iParams[])
	
	{
	enum {
		paramFuncAddress	= 0,
		paramMe,
		paramVectorSize,
		paramFreq,
		paramEpsilon,
		paramOut,
		
		paramNextLink
		};
		
	objGruyere*		me = (objGruyere*) iParams[paramMe];
	long			i,
					tLen,
					nChans;
	unsigned long	rThresh;
	double			x,
					phi,
					stepFactor;
	tBufferPtr		buf;
	tSampleVector	wTable,
					out;
	
#if __BUFFER_HAS_INUSE_FLAG__
	long			inuseState;
#endif
	
	if (me->coreObject.z_disabled) goto exit;
	
	// Need to update control-rate parameters?
	if ((float*) iParams[paramFreq] != NIL) {
		x = *((float*) iParams[paramFreq]);
		if (x != me->freq) SetFreq(me, x);
		}
	if ((float*) iParams[paramEpsilon] != NIL) {
		x = *((float*) iParams[paramEpsilon]);
		if (x != me->epsilon) SetEpsilon(me, x);
		}
	
	// Copy other parameters and some object members into registers
	i			= (long) iParams[paramVectorSize];
	out			= (tSampleVector) iParams[paramOut];
	stepFactor	= me->stepFactor;
	buf			= me->buf;
	rThresh		= me->randThresh;
	phi			= me->phi;
	
	if (buf == NIL)  {
		// No buffer~ specified, so use default cosine table
		wTable	= gCosine;
		tLen	= kTableLen;
		nChans	= 1;
		}
	
	else {
		long offset;
		
		if (buf->b_valid && (offset = me->bufOffset) < buf->b_frames) {
			wTable	= buf->b_samples + offset;
			tLen	= buf->b_frames - offset;
			
			if (tLen > kTableLen)
				tLen = kTableLen;
			
			nChans = buf->b_nchans;
			
		#if __BUFFER_HAS_INUSE_FLAG__
			inuseState		= buf->b_inuse;
			buf->b_inuse	= true;
		#endif
			}
		
		else {
			// Either buffer or offset is invalid
			// Zero output vector and punt
			do { *out++ = 0.0; } while (--i > 0);
			goto exit;
			}
		}
	
	if (stepFactor > tLen) stepFactor = fmod(stepFactor, tLen);
	
	// Do our stuff
	// We can do this a little faster with the default sine table or a single-channel table
	// of the same size
	if (nChans == 1 && tLen == kTableLen) do {
		unsigned long	rand = Taus88(NIL);
		double			samp1, samp2, frac;
		int				index;
		
		if (rand <= rThresh) {
			phi += stepFactor;
			if (phi >= (double) kTableLen) phi -= (double) kTableLen;
			}
		else {
			phi += stepFactor * (rand & kTableLenMod);
			if (phi >= tLen) phi = fmod(phi, (double) kTableLen);
			}
		
		index	= phi;							// Truncate to int
		frac	= phi - floor(phi);
		
		samp1 = wTable[index++];
		samp2 = (index < kTableLen) ? wTable[index] : wTable[0];
		
		*out++ = (1.0 - frac) * samp1 + frac * samp2;
		} while (--i > 0);
	
	else do {
		unsigned long	rand = Taus88(NIL);
		double			samp1, samp2, frac;
		int				index;
		
		if (rand <= rThresh) {
			phi += stepFactor;
			if (phi >= (double) kTableLen) phi -= (double) kTableLen;
			}
		else {
			phi += stepFactor * (rand % tLen);
			if (phi >= tLen) phi = fmod(phi, (double) kTableLen);
			}
		
		index	= phi;							// Truncate to int
		frac	= phi - floor(phi);
		
		samp1 = (index < tLen) ? wTable[index * nChans] : 0.0;
		index += 1;
		samp2 = (index < kTableLen)
					? ( (index < tLen) ? wTable[index * nChans] : 0.0 )
					: wTable[0];
		
		*out++ = (1.0 - frac) * samp1 + frac * samp2;
		} while (--i > 0);
	
	// Update object members
	me->phi	= phi;

#if __BUFFER_HAS_INUSE_FLAG__
	// Do we need to restore the state of buffer's b_inuse flag?
	// ASSERT: the following condition evaluates to true if and only if
	//				- buf was valid
	//				- we stored the previous value of buf->b_inuse
	if (wTable != gCosine)
		buf->b_inuse = inuseState;
#endif
	
exit:
	return iParams + paramNextLink;
	}
*/
