/*
	File:		twist~.c

	Contains:	Chebyshev-based waveshaping.

	Written by:	Peter Castine

	Copyright:	© 2007 Peter Castine

	Change History (most recent first):

*/


/******************************************************************************************
	Previous History
		18-Feb-2007:	First implementation.
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"
#include "MoreMath.h"
//#include "TrialPeriodUtils.h"

#pragma mark • Constants

const char	kClassName[]	= "lp.galliard~";	    		// Class name

#define LPAssistIn1			"Signal to process, list of coefficients"
#define LPAssistOut1		"Signal (Processed signal)"

/*
// Indices for STR# resource
enum {
	strIndexIn		= lpStrIndexLastStandard + 1,
	strIndexOut,

	strIndexInLeft		= strIndexIn,
	strIndexOutLeft		= strIndexOut
	};
*/
enum {
	kMaxListLen = 255
	};

#pragma mark • Type Definitions

#pragma mark • Object Structure

typedef struct {
	t_pxobject	coreObject;
	
	long		order;
	double		coeffs[kMaxListLen];
	} objPolynom;


#pragma mark • Global Variables


#pragma mark • Function Prototypes

objPolynom*	GalliardNew(t_symbol*, short, t_atom[]);

void	GalliardCoeffs(objPolynom*, t_symbol*, short, t_atom[]);
void	GalliardConstFt(objPolynom*, double);
void	GalliardConstInt(objPolynom*, long);
	
//void	GalliardDSP(objPolynom*, t_signal*[], short[]);
//int*	GalliardPerform(int[]);				// Relies on int as 32-bit. Stupid ext_proto.h
void    GalliardDSP64(objPolynom*, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void    GalliardPerform64(objPolynom*, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

void	GalliardTattle(objPolynom*);
void	GalliardAssist(objPolynom*,	tBoxPtr, long, long, char[]);
void	GalliardInfo(objPolynom*);




#pragma mark -
/*****************************  I M P L E M E N T A T I O N  ******************************/

#pragma mark • Inline Functions

#pragma mark -

/******************************************************************************************
 *
 *	main()
 *	
 *	Standard Max External Object Entry Point Function
 *	
 ******************************************************************************************/

int C74_EXPORT main(void)
	
	{
	//LITTER_CHECKTIMEOUT(kClassName);
        t_class *c;
	
	// Standard Max/MSP/Litter setup() call
	c = class_new(kClassName,						// Pointer to our class definition
			(method) GalliardNew,					// Instance creation function
			(method) dsp_free,              	// Custom deallocation function
			(short) sizeof(objPolynom),			// Class object size
			NIL,								// No menu function
			A_GIMME,							// Variable-length argument list
			0);
	
	class_dspinit(c);
	
	// Messages
	class_addmethod(c, (method) GalliardCoeffs,	"list",		A_GIMME, 0);
	class_addmethod(c, (method) GalliardConstFt, "float", A_FLOAT, 0);
	class_addmethod(c, (method) GalliardConstInt, "int", A_LONG, 0);
	
	// MSP-Level messages
	class_addmethod(c, (method) GalliardDSP64, 	"dsp64",		A_CANT, 0);

	// Informational messages
	class_addmethod(c, (method) GalliardTattle,	"dblclick",	A_CANT, 0);
	class_addmethod(c, (method) GalliardTattle,	"tattle",	A_NOTHING);
	class_addmethod(c, (method) GalliardAssist,	"assist",	A_CANT, 0);
	class_addmethod(c, (method) GalliardInfo,		"info",		A_CANT, 0);
	
	// Initialize Litter Library
	//LitterInit(kClassName, 0);
        class_register(CLASS_BOX, c);
        gObjectClass = c;
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;

	
	}


#pragma mark -
#pragma mark • Instantiation

/******************************************************************************************
 *
 *	NewGalliard(me)
 *	
 *	Uncharacteristically, we create the object first and then parse the argument list.
 *
 ******************************************************************************************/

objPolynom*
GalliardNew(
	t_symbol*	sym,
	short	iArgC,
	t_atom*	iArgV)
	
	{
	#pragma unused(sym)
	
	objPolynom*	me	= NIL;
	
	//
	// Let Max allocate us, our inlets, and our outlets
	//
	
	me = object_alloc(gObjectClass);
	if (me == NIL) {
		error("%s: insufficient memory to create object", kClassName);
		goto punt;
		}
	
	dsp_setup(&(me->coreObject), 1);		// Inlets
	outlet_new(me, "signal");

	// Set up to default state
	me->order		= 0;
	me->coeffs[0]	= 0.0;
	
	switch (iArgC) {
	case 0:
		// nothing to do;
		break;
	
	case 1:
		switch (iArgV[0].a_type) {
		case A_LONG:
			GalliardConstInt(me, iArgV[0].a_w.w_long);
			break;
		case A_FLOAT:
			GalliardConstFt(me, iArgV[0].a_w.w_float);
			break;
		default:
			break;
			}
		break;
			
	default:
		GalliardCoeffs(me, NULL, iArgC, iArgV);
		break;
		}

punt:
	return me;
	}	

#pragma mark -
#pragma mark • Utilities


#pragma mark -
#pragma mark • Object Methods

/******************************************************************************************
 *
 *	GalliardCoeffs(me, sym, iArgC, iArgV)
 *	GalliardConstFt(me, iConst)
 *	GalliardConstInt(me, iConst)
 *
 ******************************************************************************************/

void
GalliardCoeffs(
	objPolynom*	me,
	t_symbol*	sym,
	short		iArgC,
	t_atom		iArgV[])
	
	{
	#pragma unused (sym)
	
	int		order = iArgC - 1;
	double*	coeff = me->coeffs;
	
	me->order = order;
	
	do	{
		*coeff++ = AtomGetFloat(iArgV++);	// Handles ints and symbols nicely for us
		} while (--order >= 0);
	
	}

	// We could implement these directly with a bit more efficiency, but using a single
	// bottle-neck function guarantees that everything is handled consistently.
	// Particularly if we ever change the way we organize data or the evaluation algorithm.
void
GalliardConstFt(
	objPolynom* me,
	double		iConst)
	{
	t_atom a;
	
	AtomSetFloat(&a, iConst);
	GalliardCoeffs(me, NULL, 1, &a);
	}
	
void GalliardConstInt(objPolynom* me, long iConst)
	{ GalliardConstFt(me, (double) iConst); }

#pragma mark -
#pragma mark • DSP Methods

/******************************************************************************************
 *
 *	GalliardDSP(me, ioDSPVectors, iConnectCounts)
 *
 ******************************************************************************************/
/*
void
GalliardDSP(
	objPolynom*	me,
	t_signal*	ioDSPVectors[],
	short		connectCounts[])
	
	{
	enum {
		inletSigIn		= 0,
		outletSigOut
		};
	
	#pragma unused(connectCounts)
	
	dsp_add(
		GalliardPerform, 4,
		me, (long) ioDSPVectors[inletSigIn]->s_n,
		ioDSPVectors[inletSigIn]->s_vec,
		ioDSPVectors[outletSigOut]->s_vec
		);
	
	}
*/

void    GalliardDSP64(objPolynom *me, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags) {
    
    object_method(dsp64, gensym("dsp_add64"), me, GalliardPerform64, 0, NULL);
    
}



/******************************************************************************************
 *
 *	GalliardPerform(iParams)
 *
 *	Parameter block for PerformSync contains 4 values:
 *		- Address of this function
 *		- The performing galliard~ object
 *		- Vector size
 *		- output signal
 *
 ******************************************************************************************/
/*
int*
GalliardPerform(
	int iParams[])
	
	{
	enum {
		paramFuncAddress	= 0,
		paramMe,
		paramVectorSize,
		paramIn,
		paramOut,
		
		paramNextLink
		};
	
	long			vecCounter;
	tSampleVector	inSig,
					outSig;
	int				order;
	double*			coeffs;
	objPolynom*		me = (objPolynom*) iParams[paramMe];
	
	if (me->coreObject.z_disabled) goto exit;
	
	// Copy parameters into registers
	vecCounter	= (long) iParams[paramVectorSize];
	inSig		= (tSampleVector) iParams[paramIn];
	outSig		= (tSampleVector) iParams[paramOut];
	
	// Do our stuff
	order	= me->order;
	coeffs	= me->coeffs;
	
	// Watch out! EvalPoly() is currently unsafe for 0th-order functions
	if (order == 0) do {
		*outSig++ = *coeffs;
		} while (--vecCounter > 0);
	else do {
		*outSig++ = EvalPoly((double) (*inSig++), coeffs, order);
		} while (--vecCounter > 0);
	
exit:
	return iParams + paramNextLink;
	}

*/



void    GalliardPerform64(objPolynom *me, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
    
    long			vecCounter;
    tSampleVector	inSig, outSig;
    int				order;
    double*			coeffs;
    
    if (me->coreObject.z_disabled) return;
    
    // Copy parameters into registers
    vecCounter	= sampleframes;
    inSig		= ins[0];
    outSig		= outs[0];
    
    // Do our stuff
    order	= me->order;
    coeffs	= me->coeffs;
    
    // Watch out! EvalPoly() is currently unsafe for 0th-order functions
    if (order == 0) do {
        *outSig++ = *coeffs;
    } while (--vecCounter > 0);
    else do {
        *outSig++ = EvalPoly((*inSig++), coeffs, order);
    } while (--vecCounter > 0);
    
}

/******************************************************************************************
 *
 *	GalliardTattle(me)
 *
 * 	Tell all we know.
 *
 ******************************************************************************************/

void
GalliardTattle(
	objPolynom*	me)
	
	{
	int	i	= me->order;
	double*	coeff = me->coeffs;
	
	post("%s state:", kClassName);
	post("  Coefficients:");
	do	{ PostFloat(*coeff++); } while (--i >= 0);
	
	}

/******************************************************************************************
 *
 *	GalliardAssist
 *
 ******************************************************************************************/

void
GalliardAssist(
	objPolynom*	me,
	void*		box,
	long		iDir,
	long		iArgNum,
	char		oCDestStr[])
	
	{
	#pragma unused(box)
	
	//LitterAssist(iDir, iArgNum, strIndexInLeft, strIndexOutLeft, oCDestStr);
        if (iDir == ASSIST_INLET)
            sprintf (oCDestStr, LPAssistIn1);
        else
            sprintf (oCDestStr, LPAssistOut1);

	}

void GalliardInfo(objPolynom* me)
	{ LitterInfo(kClassName, (tObjectPtr) me, (method) GalliardTattle); }

	

	
