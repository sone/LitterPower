/*
	File:		alea.c

	Contains:	Litter replacement for the RTC alea abstraction.

	Written by:	Peter Castine

	Copyright:	© 2006 Peter Castine

	Change History (most recent first):

*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"
#include "Taus88.h"

#pragma mark • Constants

const char*	kClassName		= "lp.ale";			// Class name

// Assistance strings
#define LPAssistIn		"List, Bang, Set"
#define LPAssistOut		"An element taken at random from the input list"

	// Have to use enum because GCC and some other compilers do not consider const int
	// to be a constant expression.
enum {
	kMaxListLen			= 255
	};

/*
	// Indices for STR# resource
enum {
	strIndexLeftIn			= lpStrIndexLastStandard + 1,
	strIndexLeftOut
	};
*/

#pragma mark • Type Definitions



#pragma mark • Object Structure

typedef struct {
	t_object		coreObject;
	
	tOutletPtr		outletReset;
	
	tTaus88DataPtr	tausData;
	
	t_atom			items[kMaxListLen];
	short			itemCount,
					itemsLeft;
	
	} objAle;


#pragma mark • Global Variables

#pragma mark • Function Prototypes

	// Class message functions
objAle*		AleNew(long);
void		AleFree(objAle*);

	// Object message functions
static void AleBang(objAle*);
static void AleInt(objAle*, long);
static void AleFloat(objAle*, double);
static void AleList(objAle*, t_symbol*, short, t_atom[]);
static void AleSet(objAle*, t_symbol*, short, t_atom[]);
static void AleAnything(objAle*, t_symbol*, short, t_atom[]);
static void AleTattle(objAle*);
static void	AleAssist(objAle*, void*, long, long, char*);
static void	AleInfo(objAle*);

#pragma mark -
/*****************************  I M P L E M E N T A T I O N  ******************************/

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
			(method) AleNew,				// Instantiation method  
			(method) AleFree,				// Custom deallocation function
			(short) sizeof(objAle),			// Class object size
			NIL,							// No menu function
			A_DEFLONG,						// If non-zero initialize & maintain a
			0);								// private seed pool
	
	// Messages
	class_addmethod(c, (method) AleBang,    "bang", 0);
	class_addmethod(c, (method) AleInt,     "int",      A_LONG, 0);
	class_addmethod(c, (method) AleFloat,   "float",    A_FLOAT, 0);
	class_addmethod(c, (method) AleList,	"list",     A_GIMME, 0);
	class_addmethod(c, (method) AleAnything,"anything", A_GIMME, 0);
	class_addmethod(c, (method) AleSet,		"set",		A_GIMME, 0);
	class_addmethod(c, (method) AleAssist,	"assist",	A_CANT, 0);
	class_addmethod(c, (method) AleInfo,	"info",		A_CANT, 0);
	class_addmethod(c, (method) AleTattle,	"tattle",	A_NOTHING);
	class_addmethod(c, (method) AleTattle,	"dblclick",	A_CANT, 0);
	
	// Initialize Litter Library
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
 *	AleNew(iSym, iArgCount, iArgVec)
 *	AleFree(me)
 *
 ******************************************************************************************/

objAle*
AleNew(
	long iSeed)
	
	{
	objAle*	me = NIL;
	int				i;
	
	// Create object
	me = object_alloc(gObjectClass);
		if (me == NIL) goto punt;			// Poor man's exception handling
	
	// Add inlets and outlets
	me->outletReset = bangout(me);
	outlet_new(me, NIL);					// Access through me->coreObject.o_outlet
	
	// Initialize object members
	me->tausData	= NIL;					// Try to allocate later, if requested by user
	me->itemCount	= 0;
	me->itemsLeft	= 0;
	
	// Slightly anal-retentive, but safety first
	for (i = 0; i < kMaxListLen; i += 1) {
		me->items[i].a_type		= A_NOTHING;
		me->items[i].a_w.w_long	= 0;
		}
	
	// Now that we're squeaky clean, check if we need to allocate our own seed base
	if (iSeed != 0)
		me->tausData = Taus88New(iSeed);
	
punt:
	return me;
	}

void AleFree(objAle* me)
	{
	Taus88Free(me->tausData);		// Taus88Free() is null-safe
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	AleBang(me)
 *	AleSet(me, sym, iArgCount, iArgVec)
 *
 *	AleInt(me, iVal)
 *	AleFloat(me, iVal)
 *	AleList(me, sym, iArgCount, iArgVec)
 *	
 ******************************************************************************************/

void
AleBang(
	objAle* me)
	
	{
	UInt32	itemsLeft,
			chosenOne;
	
	if (me->itemCount <= 0)
		return;
	
	itemsLeft = me->itemsLeft;
	if (itemsLeft <= 0) {
		itemsLeft = me->itemCount;
		outlet_bang(me->outletReset);
		}
	 
	 chosenOne = (itemsLeft > 1)
					? ((Taus88(me->tausData) & 0x0000ffff) * itemsLeft) >> 16
					: 0;
	 	// ASSERT: 0 <= chosenOne < itemsLeft <= me->itemCount
	
	switch (me->items[chosenOne].a_type) {
	case A_LONG:
		outlet_int(me->coreObject.o_outlet, me->items[chosenOne].a_w.w_long);
		break;
	case A_FLOAT:
		outlet_float(me->coreObject.o_outlet, me->items[chosenOne].a_w.w_float);
		break;
	case A_SYM:
		outlet_anything(me->coreObject.o_outlet, me->items[chosenOne].a_w.w_sym, 0, NIL);
		break;
	default:
		// Can't happen, nothing to do
		break;
		}
	
	// Put the chosen one one the side so it doesn't get used again in this cycle
	me->itemsLeft = itemsLeft -= 1;
	if (chosenOne < itemsLeft)
		AtomSwap(me->items + chosenOne, me->items + itemsLeft);
	
	}

void
AleSet(
	objAle*	me,
	t_symbol*			sym,					// Not used, must be gensym("set")
	short			iArgCount,
	t_atom			iArgVec[])
	
	{
	#pragma unused(sym)	
	
	int i;
	
	// Just in case some non-standard object sends a list longer than the standard max
	if (iArgCount > kMaxListLen) {
		iArgCount = kMaxListLen;
		error("%s: items truncated", kClassName);
		}
	
	for (i = 0; i < iArgCount; i += 1)
		me->items[i] = iArgVec[i];
	
	me->itemCount = iArgCount;
	me->itemsLeft = iArgCount;
	
	}


void AleInt(objAle* me, long iVal)
	{
	t_atom a;
	
	//AtomSetLong(&a, iVal);
        atom_setlong(&a, iVal);
	
	AleSet(me, NIL, 1, &a);
	AleBang(me);
	
	}

void AleFloat(objAle* me, double iVal)
	{
	t_atom a;
	
	//AtomSetFloat(&a, iVal);
        atom_setfloat(&a, iVal);
	
	AleSet(me, NIL, 1, &a);
	AleBang(me);
	
	}
	

void
AleList(
	objAle*	me,
	t_symbol*			sym,										// unused, must be "list"
	short			iArgCount,
	t_atom			iArgVec[])
	
	{
	#pragma unused(sym)
	
	AleSet(me, NIL, iArgCount, iArgVec);
	AleBang(me);
	
	}

/******************************************************************************************
 *
 *	AleAnything(me)
 *
 *	Sort of special case: we treat arbitrary messages with parameter lists as if they were
 *	a list beginning with a t_symbol.
 *
 ******************************************************************************************/

void
AleAnything(
	objAle*	me,
	t_symbol*			iSym,
	short			iArgCount,
	t_atom			iArgVec[])
	
	{
	int i;
	
	// Just in case some non-standard object sends a list longer than the standard max
	if (iArgCount >= kMaxListLen) {
		iArgCount = kMaxListLen - 1;
		error("%s: items truncated", kClassName);
		}
	
	//AtomSetSym(&me->items[0], iSym);
        atom_setsym(&me->items[0], iSym);
	
	for (i = 0; i < iArgCount; i += 1)
		me->items[i+1] = iArgVec[i];
	
	me->itemCount = iArgCount + 1;
	me->itemsLeft = me->itemCount;
	
	AleBang(me);
	
	}




/******************************************************************************************
 *
 *	AleTattle(me)
 *
 * 	Tell all we know.
 *
 ******************************************************************************************/

void
AleTattle(
	objAle*	me)
	
	{
	
	// Object state
	post("%s status", kClassName);
	
	// ... and then stuff specific to Ale
	post("  chosing from %ld items", (long) me->itemCount);
	post("  %ld items left in pool", (long) me->itemsLeft);
	
	}

/******************************************************************************************
 *
 *	AleAssist(me, iBox, iDir, iArgNum, oCStr)
 *	AleInfo(me)
 *
 ******************************************************************************************/

void
AleAssist(
	objAle*	me,
	tBoxPtr			box,				// unused
	long			iDir,
	long			iArgNum,
	char*			oCStr)
	
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexLeftIn, strIndexLeftOut, oCStr);
        if (iDir == ASSIST_INLET)
            sprintf (oCStr, LPAssistIn);
        else
            sprintf (oCStr, LPAssistOut);
        
	
	}

void AleInfo(objAle* me)
	{ LitterInfo(kClassName, (tObjectPtr) me, (method) AleTattle); }


