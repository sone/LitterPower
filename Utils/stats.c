/*
	File:		stats.c

	Contains:	Track basic statistics on incoming data: count, minimum, maximum, mean,
				standard deviation, skew, and kurtosis.

	Written by:	Peter Castine

	Copyright:	© 2001-2002 Peter Castine

	Change History (most recent first):

         <9>     11–1–04    pc      Update for modified LitterInit()
         <8>      8–1–04    pc      Update for Windows.
         <7>    17–12–03    pc      Update for Windows.
         <6>   8–16–2003    pc      Forgot to remove legacy finder_addclass() call while adding
                                    LitterAddClass(). Fixed.
         <5>    6–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <4>  30–12–2002    pc      Add object version to StaceyInfo()
         <3>  30–12–2002    pc      Use 'STR#' resource instead of faux 'Vers' resource for storing
                                    version information used at run-time.
         <2>  28–11–2002    pc      Tidy up after initial check in.
         <1>  28–11–2002    pc      Initial check in.
         
		 2-Apr-2001:		First implementation.
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"
//#include "TrialPeriodUtils.h"

#include <math.h>

#pragma mark • Constants

const char*	kClassName		= "lp.stacey";			// Class name

// Assistance strings
#define LPAssistIn1			"Float or Int (Number is added to statistics)"
#define LPAssistOut1		"Int (Count)"
#define LPAssistOut2		"Float (Minimum)"
#define LPAssistOut3		"Float (Maximum)"
#define LPAssistOut4		"Float (Mean)"
#define LPAssistOut5		"Float (Standard Deviation)"
#define LPAssistOut6		"Float (Skew)"
#define LPAssistOut7		"Float (Kurtosis)"

/*
	// Indices for STR# resource
enum {
	strIndexTheInlet		= lpStrIndexLastStandard + 1,
	
	strIndexOutCount,
	strIndexOutMin,
	strIndexOutMax,
	strIndexOutMean,
	strIndexOutStdDev,
	strIndexOutSkew,
	strIndexOutKurtosis
	};
*/
	// Indices for collected statistics
enum {
	statSum			= 0,
	statSum2,				// Sum of squares
	statSum3,				// Sum of cubes
	statSum4,				// Sum of 4th power.
	
	statArraySize
	};

#pragma mark • Type Definitions

#pragma mark • Object Structure

typedef struct {
	t_object		coreObject;
	voidPtr		minOutlet,
				maxOutlet,
				meanOutlet,
				stdDevOutlet,
				skewOutlet,
				kurtosisOutlet;
	
	unsigned long
				count,
				bufSize,
				curElem;
	// Use double for all of the following. We may be dealing with long integers
	// (which exceed the range of 32-bit fp). The additional accuracy is also a good
	// idea for the intermediate calculations stored in the data vector.
	double		min,
				max;
	double		data[statArraySize];
	double*		buffer;
	} tLStats;


#pragma mark • Global Variables


#pragma mark • Function Prototypes

	// Class message functions
void*	StaceyNew(long);
void	StaceyFree(tLStats*);

	// Object message functions
static void StaceyBang(tLStats*);
static void StaceyFloat(tLStats*, double);
static void StaceyInt(tLStats*, long);
static void StaceyList(tLStats*, t_symbol*, short, t_atom*);
static void StaceyRemove(tLStats*, t_symbol*, short, t_atom*);
static void StaceyClear(tLStats*);
static void StaceyClearBang(tLStats*);
static void StaceyTattle(tLStats*);
static void	StaceyAssist(tLStats*, void* , long , long , char*);
static void	StaceyInfo(tLStats*);


#pragma mark -
/*****************************  I M P L E M E N T A T I O N  ******************************/

#pragma mark • Inline Functions

#pragma mark -

/******************************************************************************************
 *
 *	main()
 *	
 *	Standar Max External Object Entry Point Function
 *	
 ******************************************************************************************/

int C74_EXPORT main(void)
	
	{
	//LITTER_CHECKTIMEOUT(kClassName);
        t_class *c;
	
	// Standard Max setup() call
	c = class_new(	kClassName,					// Pointer to our class definition
			(method) StaceyNew,			    // Instantiation method
			(method) StaceyFree,			// Deallocation method
			(short) sizeof(tLStats)	,		// Class object size
			NIL,							// No menu function
			A_DEFLONG,						// 1 Argument: size of statistics window
											//		for running stats
			0);
	
	// Messages
	class_addmethod(c, (method) StaceyBang, "bang", 0);
	class_addmethod(c, (method) StaceyInt, "int", A_LONG, 0);
	class_addmethod(c, (method) StaceyFloat, "float", A_FLOAT, 0);
	class_addmethod(c, (method) StaceyList, "list", A_GIMME, 0);
	class_addmethod(c, (method) StaceyRemove,	"remove", A_GIMME, 0);
	class_addmethod(c, (method) StaceyClear,		"clear",		A_NOTHING);
	class_addmethod(c, (method) StaceyClearBang,	"clearbang",	A_NOTHING);
	class_addmethod(c, (method) StaceyAssist,		"assist",		A_CANT, 0);
	class_addmethod(c, (method) StaceyInfo,		"info",			A_CANT, 0);
	class_addmethod(c, (method) StaceyTattle,		"dblclick", 	A_CANT, 0);
	class_addmethod(c, (method) StaceyTattle,		"tattle",	 	A_NOTHING);
	
	// Initialize Litter Library
	//LitterInit(kClassName, 0);
        class_register(CLASS_BOX, c);
        gObjectClass = c;
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;

	}

#pragma mark -
#pragma mark • Class Message Handlers

/******************************************************************************************
 *
 *	StaceyNew(iOracle)
 *
 ******************************************************************************************/

void*
StaceyNew(
	long	iWindow)
	
	{
	const long kMinWindow = 2;			// Even this is a pretty meaningless window
	
	tLStats*	me			= NIL;
	
	// Let Max allocate us. LitterStats has only the default inlet
	me = object_alloc(gObjectClass);
	if (me == NIL) goto punt;
	
	// Zero pointers and other stuff we may or may not allocate latter
	me->kurtosisOutlet	= NIL;
	me->skewOutlet		= NIL;
	me->stdDevOutlet	= NIL;
	me->meanOutlet		= NIL;
	me->maxOutlet		= NIL;
	me->minOutlet		= NIL;
	me->buffer			= NIL;
	me->bufSize			= 0;
	
	// Allocate buffer carefully, this has the potential to be a memory killer
	if (iWindow > kMinWindow) {
		double* buffer = (double*) sysmem_newptrclear(iWindow * sizeof(double));
		if (buffer != NIL) {
			me->bufSize	= iWindow;
			me->buffer	= buffer;
			}
		else error("%s: not enough memory for window with %ld elements", kClassName, iWindow);
		}
	else if (iWindow != 0)	// Stupid things user will do. Kvetch.
		error("%s: invalid window size ignored", kClassName);
	
	// Add 7 outlets, right to left.
	// Note that for some small window sizes not all outlets are valid
	switch(me->bufSize) {				// ASSERT: (me->bufSize >=5 || me->bufSize == 0)
	default:
		me->kurtosisOutlet	= floatout(me);
		// fall into next case
	case 4:
		me->skewOutlet		= floatout(me);
		// fall through...
	case 3:
		me->stdDevOutlet	= floatout(me);
		// fall through...
	case 2:
		me->meanOutlet		= floatout(me);
		me->maxOutlet		= floatout(me);
		me->minOutlet		= floatout(me);
		intout(me);									// Count outlet is accessed through 
		break;										// me->coreObject.o_outlet
		}
	
	// Reset all counters, running totals, etc.
	StaceyClear(me);
	
punt:
	return me;
	}

void
StaceyFree(tLStats* me)
	
	{
	if (me->buffer != NIL) {
		sysmem_freeptr(me->buffer);
		me->buffer = NIL;	// Should be superfluous in this context, but better safe...
		}
	}


#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	StaceyBang(me)
 *
 ******************************************************************************************/

void
StaceyBang(
	tLStats* me)
	
	{
	unsigned long	count		= me->count;			// Copy into register
	double			mean,								// These must be initializied, but
					stdDev,								// to be safe, do this after
					skew,								// EnterCallback();
					kurtosis;
	
	
	// Default values for mean, etc., in case there is not yet enough data
	// to calculate
	mean		= 0.0;
	stdDev		= 0.0;
	skew		= 0.0;
	kurtosis	= 0.0;
	
	// Evil chain of gotos.
	// Doing this with nested ifs would be just as ugly, if not more so.
	{
	double	sum, sum2, sum3,				// Stored values will be copied into registers
			mean2, mean3,					// Buffer µ^2 and µ^3
			var,							// Var is square of Std. Dev
			kurTemp1, kurTemp2;				// Two temporary terms we use in calculating kurtosis.
	
	if (count == 0) goto puntStats;			// Need at least one datum for mean
		// We use sum a lot; copy into a register.
	sum		= me->data[statSum];
	mean	= sum / count;
	
	if (count == 1) goto puntStats;			// Need at least two elements for stdDev
		// Grab some intermediate values
	sum2	= me->data[statSum2];
	mean2	= mean * mean;
		// Variation =  ∑((X-µ)^2) / (N - 1)
		// Use N-1 as denominator for estimate from sample population
		// (Read up a stat text book for details of why)
		// Calc numerator first
		// NB:		∑((X-µ)^2)	= ∑(X^2 - 2µX + µ^2)
		//						= ∑X^2 - 2µ(∑X) + N(µ^2)
		// Calc nmuerator first. Start with middle term and work out
	var	= -mean * sum;
	var += var;								// Addition is always cheaper than "times 2"
	var += sum2 + count * mean2;
		// Divide out by denominator
	var /= count - 1;
	if (var == 0.0) goto puntStats;			// This can happen, in which case all other
											// stats are meaningless and we get NANs.
		// StdDev is defined as sqrt(variance)
	stdDev = sqrt(var);
	
	if (count == 2) goto puntStats;			// Need at least three elements for skew
		// Grab some intermediate values
	sum3	= me->data[statSum3];
	mean3	= mean * mean2;
		// Skew = ( ∑(X-µ)^3 ) / (N * stdDev^3)
		// Calc numerator first...
		// NB:		∑(X-µ)^3	= ∑(X^3 - 3µX^2 + 3µ^2X - µ^3)
		//						= ∑X^3 - 3µ(∑X^2) + 3µ^2(∑X) - Nµ^3
		// Start with the inner terms
	skew =  mean2 * sum - mean * sum2;
	skew += skew + skew;					// Addition is still cheaper than multiply
		// Add outer terms
	skew += sum3 - count * mean3;
		// ...then divide by denominator.
		// We still have stdDev^2 in variance, saving a multiply
	skew /= count * var * stdDev;
	
	if (count == 3) goto puntStats;			// Need at least four for kurtosis
		// Kurtosis = (( ∑(X-µ)^4 ) / (N * stdDev^4)) - 3
		// The 3 is a magic normalization constant (cf. those statistic textbooks)
		// Again, multiply out the numerator:
		//			∑(X-µ)^4	= ∑(X^4 - 4µX^3 + 6µ^2X^2 - 4µ^3X + µ^4)
		//						= ∑X^4 - 4µ(∑X^3) + 6µ^2(∑X^2) - 4µ^3(∑X) + Nµ^4
		// Again, Calc numerator first.
		// We use two temp variable for the inner terms, again to resolve some
		// multiplications into additions.
	kurTemp1 = mean2 * sum2;
	kurTemp2 = mean * sum3 + mean3 * sum;
	kurTemp1 += kurTemp1 + kurTemp1 - kurTemp2 - kurTemp2;
		// ASSERT: kurTemp1 == - 2µ(∑X^3) + 3µ^2(∑X^2) - 2µ^3(∑X)
	kurtosis = me->data[statSum4] + kurTemp1 + kurTemp1 + count * mean2 * mean2;
		// Was that hard?
		// Now just divide out the denominator
	kurtosis /= count * var * var;
		// and subtract the magical 3
	kurtosis -= 3.0;
	
	}
puntStats:
	// End of evil goto chain
	
	switch (me->bufSize) {
		// Fall through switching...
	default:
		outlet_float(me->kurtosisOutlet, kurtosis);
	case 4:
		outlet_float(me->skewOutlet, skew);
	case 3:
		outlet_float(me->stdDevOutlet, stdDev);
	case 2:
		outlet_float(me->meanOutlet, mean);
		outlet_float(me->maxOutlet, me->max);
		outlet_float(me->minOutlet, me->min);
		outlet_int	(me->coreObject.o_outlet, count);
		break;
		}
	
	}


/******************************************************************************************
 *
 *	UndoStats(me, iVal)
 *	DoStats(me, iVal)
 *	StaceyInt(me, iVal)
 *	StaceyFloat(me, iVal)
 *	StaceyList(me, iVal)
 *
 * 	DoStats() does the brute work; StaceyInt() and StaceyFloat() just typecast to double and
 *	call it.
 *	StaceyList iterates through the list and calls DoStats()
 *
 ******************************************************************************************/

	static double ScanForMin(const double* iBuf, const long iElemCount, const long iIgnoreElem)
		{
		long	curElem	= (iIgnoreElem == 0) ? 1 : 0;
		double	min 	= iBuf[curElem];
		
		while (++curElem < iElemCount) {
			if (min > iBuf[curElem] && curElem != iIgnoreElem) min = iBuf[curElem];
			}
		
		return min;
		}
	
	static double ScanForMax(const double* iBuf, const long iElemCount, const long iIgnoreElem)
		{
		long	curElem	= (iIgnoreElem == 0) ? 1 : 0;
		double	max		= iBuf[curElem];
		
		while (++curElem < iElemCount) {
			if (max < iBuf[curElem] && curElem != iIgnoreElem) max = iBuf[curElem];
			}
		
		return max;
		}
	

static void
UndoStats(
	tLStats*	me,
	double		iVal)
	
	{
	int		i;
	double*	s;
	double	v;
	
	if (me->count <= 0)				// Sanity check
		return;						// Quick punt
	
	// Revise sums of powers
	i = statArraySize;
	s = me->data;
	v = 1.0;
	while (i-- > 0) {
		v *= iVal; *s++ -= v;
		}
	
	// Revise min/max
	if (me->buffer) {
		if (me->min == iVal)
			me->min = ScanForMin(me->buffer, me->count, me->curElem);
		if (me->max == iVal)
			me->max = ScanForMax(me->buffer, me->count, me->curElem);
		}
	else {
		// Cheesy way of dealing with this condition, but what else can we do??
		if (me->min == iVal)	me->min = me->max;
		if (me->max == iVal)	me->max = me->min;
		}
		
	// Revise count.
	me->count -= 1;
		
	}

static void
DoStats(
	tLStats*	me,
	double		iVal)
	
	{
	int		i;
	double*	s;
	double	v;
	
	if (me->buffer) {
		me->curElem += 1;
		if (me->curElem >= me->bufSize)
			me->curElem = 0;
		
		if (me->count >= me->bufSize)	
			UndoStats(me, me->buffer[me->curElem]);
		me->buffer[me->curElem] = iVal;
		}
	
	// Update min/max; count is updated as side effect.
	if (me->count++ == 0)
		me->min = me->max = iVal;
	else {
		if		(iVal < me->min) me->min = iVal;
		else if (iVal > me->max) me->max = iVal;
		}
	
	i	= statArraySize;
	s	= me->data;
	v	= 1.0;
	while (i-- > 0) {
		v *= iVal; *s++ += v;
		}
	
	}

void
StaceyInt(
	tLStats*	me,
	long		iVal)
	{
	DoStats(me, (double) iVal);
	StaceyBang(me);
	}

void
StaceyFloat(
	tLStats*	me,
	double		iVal)
	{
	DoStats(me, iVal);
	StaceyBang(me);
	}

void
StaceyList(
	tLStats*	me,
	t_symbol*		sym,				// unused, must be "list"
	short		iArgCount,
	t_atom*		iAtoms)
	
	{
	#pragma unused(sym)
	/*
	while (iArgCount-- > 0) {
     
		switch (iAtoms->a_type) {
		case A_LONG:
			DoStats(me, (double) iAtoms->a_w.w_long);
			break;
		case A_FLOAT:
			DoStats(me, (double) iAtoms->a_w.w_float);
			break;
		default:
			error("%s given list with invalid atom: ", kClassName);
			postatom(iAtoms);
			break;
			}
		
		iAtoms += 1;
		}*/
        
    while (iArgCount-- > 0) {
        switch (atom_gettype(iAtoms)) {
            case A_LONG:
                DoStats(me, (double) atom_getlong(iAtoms));
                break;
            case A_FLOAT:
                DoStats(me, (double) atom_getfloat(iAtoms));
                break;
            default:
                object_error((t_object*)me, "given list with invalid atom: ");
                postatom(iAtoms);
                break;
        }
        
        iAtoms += 1;
    }
	
	StaceyBang(me);
	
	}

void
StaceyRemove(
	tLStats*	me,
	t_symbol*		sym,				// unused, must be "remove"
	short		iArgCount,
	t_atom*		iAtoms)
	
	{
	#pragma unused(sym)
	
	if (me->buffer == NIL) {
		while (iArgCount-- > 0) {
			if (me->count <= 0) {
				object_error((t_object*)me, "can't remove from empty data set");
				break;
				}
			switch (atom_gettype(iAtoms)) {
			case A_LONG:
				UndoStats(me, (double) atom_getlong(iAtoms));
				break;
			case A_FLOAT:
				UndoStats(me, (double) atom_getfloat(iAtoms));
				break;
			default:
				object_error((t_object*)me, "LitterStats given list with invalid atom: ");
				postatom(iAtoms);
				break;
				}
			
			
			iAtoms += 1;
			}
		
		StaceyBang(me);
		}
	else object_error((t_object*)me, "can't remove data in running stats mode");
	
	
	}

/******************************************************************************************
 *
 *	StaceyClear(me)
 *	StaceyClearBang(me)
 *
 * 	Reset all values. ClearBang also bangs 0 out all outlets.
 *
 ******************************************************************************************/

void
StaceyClear(
	tLStats*	me)
	
	{
	long i;
	
	
	i = statSum4;
	do { me->data[i] = 0.0; } while (--i >= statSum);
	
	me->max		= 0.0;
	me->min		= 0.0;
	me->count	= 0;
	me->curElem = -1;
	
	}

void StaceyClearBang(
	tLStats*	me)
	
	{
	StaceyClear(me);
	StaceyBang(me);
	}

/******************************************************************************************
 *
 *	StaceyTattle(me)
 *
 * 	Tell all we know.
 *
 ******************************************************************************************/

void
StaceyTattle(
	tLStats*	me)
	
	{
	
	object_post((t_object*)me, "state:");
	if (me->buffer)
		post("  calculating running stats on %ld elements", me->bufSize);
	post("  Count: %ld", me->count);
	post("  Range: %f to %f", me->min, me->max);
	post("  Sum: %f", me->data[statSum]);
	post("  Sum of squares: %f", me->data[statSum2]);
	post("  Sum of cubes: %f", me->data[statSum3]);
	post("  Sum of 4th powers: %f", me->data[statSum4]);
	
	}

/******************************************************************************************
 *
 *	StaceyAssist()
 *	StaceyInfo(me)
 *	StaceyVers(me)
 *
 *	Generic implementations
 *
 ******************************************************************************************/

void StaceyAssist(tLStats* me, void* box, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexTheInlet, strIndexOutCount, oCStr);
        if (iDir == ASSIST_INLET) {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistIn1); break;
            }
        }
        else {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistOut1); break;
                case 1: sprintf (oCStr, LPAssistOut2); break;
                case 2: sprintf (oCStr, LPAssistOut3); break;
                case 3: sprintf (oCStr, LPAssistOut4); break;
                case 4: sprintf (oCStr, LPAssistOut5); break;
                case 5: sprintf (oCStr, LPAssistOut6); break;
                case 6: sprintf (oCStr, LPAssistOut7); break;
            }
            
        }
	}

void StaceyInfo(tLStats* me)
	{ LitterInfo(kClassName, &me->coreObject, (method) StaceyTattle); }

