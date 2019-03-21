/*
	File:		flim.c

	Contains:	Floating-point interval mutation. Used also for ints (produces float values,
				but these can be truncated or otherwise rounded by other objects).

	Written by:	Peter Castine

	Copyright:	© 2002 Peter Castine. All rights reserved.

	Change History (most recent first):

         <4>     10–1–04    pc      Update for Windows, using new LitterLib calls.
         <3>    7–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <2>  30–12–2002    pc      Add object version to DoInfo()
         <1>  30–12–2002    pc      Initial Check-In

	Prior History:
		13-Jan-2002:	Wrapped integer (intim) and floating-point (flim) functionality
						into a single "[numeric] value interval mutator": vim
						Numerous bug fixes, etc.
		15-Nov-2000:	First implementation (based on previous Mutator~.c)
									

*/

// Assistance strings
#define LPAssistIn1			"Float or Int (Source). Lots of other messages."
#define LPAssistIn2			"Float or Int (Target)"
#define LPAssistIn3			"Float in [0.0 ... 1.0] (Mutation Index, mu )"
#define LPAssistIn4			"Float in [0.0 ... 1.0) (Clumping Factor, pi )"
#define LPAssistIn5			"Float in [-1.0 ... 1.0] (Delta Emphasis, d )"
#define LPAssistOut1		"Float (Mutant)"



#pragma mark • Include Files

//#include "TrialPeriodUtils.h"
#include "imLib.h"


#pragma mark • Constants

const char*	kClassName		= "lp.vim";				// Class name


#pragma mark • Function Prototypes

void*	NewMutator(t_symbol*, short, t_atom*);

	// Various Max messages
void	FlimAssist(tMutator*, void* , long , long , char*);
void	FlimInfo(tMutator*);
void	FlimTattle(tMutator*);

	// flim-specific messages
static void	FlimSource(tMutator*, double);
static void FlimSourceInt(tMutator*, long);
static void	FlimTarget(tMutator*, double);
static void FlimTargetInt(tMutator*, long);
static void FlimSet(tMutator*, double);
static void	FlimClumpStrict(tMutator*);
static void	FlimClumpTight(tMutator*, long);
static void FlimClumpHard(tMutator*, long);

static void FlimOmegaInt(tMutator*, long);
static void FlimPiInt(tMutator*, long);
static void FlimDeltaInt(tMutator*, long);

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
			(method) NewMutator,		// Instance creation function
			NIL,						// No custom deallocation function
			(short) sizeof(tMutator),	// Class object size
			NIL,						// No menu function
			A_GIMME,					// We parse our arguments
			0);		
	
	// Very standard messages
	class_addmethod(c,(method) DoBang, "bang", 0);
	class_addmethod(c,(method) FlimSource, "float", A_FLOAT, 0);
	class_addmethod(c,(method) FlimSourceInt, "int", A_LONG, 0);
	class_addmethod(c,(method) FlimTarget, "ft1", A_FLOAT, 0);
	class_addmethod(c,(method) FlimTargetInt, "in1", A_LONG, 0);
	class_addmethod(c,(method) DoOmega, "ft2", A_FLOAT, 0);
	class_addmethod(c,(method) FlimOmegaInt, "in2", A_LONG, 0);
	class_addmethod(c,(method) DoPi, "ft3", A_FLOAT, 0);
	class_addmethod(c,(method) FlimPiInt, "in3", A_LONG, 0);
	class_addmethod(c,(method) DoDelta, "ft4", A_FLOAT, 0);
	class_addmethod(c,(method) FlimDeltaInt, "in4", A_LONG, 0);
	
	// Fairly standard Max messages
	class_addmethod(c,(method) FlimAssist,		"assist",	A_CANT, 0);
	class_addmethod(c,(method) FlimInfo,			"info",		A_CANT, 0);
	class_addmethod(c,(method) FlimTattle,		"dblclick",	A_CANT, 0);
	class_addmethod(c,(method) FlimTattle,		"tattle",	A_NOTHING);
	
	// Vaguely standard messages
	class_addmethod(c,(method) DoClear,			"clear",	A_NOTHING);
	class_addmethod(c,(method) FlimSet,			"set",		A_DEFFLOAT, 0);
	
	//Our messages
	class_addmethod(c,(method) DoUSIM,			"usim",		A_NOTHING);
	class_addmethod(c,(method) DoISIM,			"isim",		A_NOTHING);
	class_addmethod(c,(method) DoUUIM,			"uuim",		A_NOTHING);
	class_addmethod(c,(method) DoIUIM,			"iuim",		A_NOTHING);
	class_addmethod(c,(method) DoWCM,			"wcm",		A_NOTHING);
	class_addmethod(c,(method) DoLCM,			"lcm",		A_NOTHING);
	
	class_addmethod(c,(method) DoAbsInt,			"abs",		A_NOTHING);
	class_addmethod(c,(method) DoRelInt,			"rel",		A_DEFFLOAT, 0);
	
	class_addmethod(c,(method) FlimClumpStrict,	"strict",	A_NOTHING);
	class_addmethod(c,(method) FlimClumpTight,	"tight",	A_DEFLONG, 0);
	class_addmethod(c,(method) FlimClumpHard,	"hard",		A_DEFLONG, 0);
	
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
 *	NewMutator(iSelector, iArgCount, iArgVector)
 *
 ******************************************************************************************/

void*
NewMutator(
	t_symbol* iSelector,
	short	iArgCount,
	t_atom*	iArgVector)
	
	{
	Boolean			gotInitType		= false,		// The gotInitXXX variables are for 
					gotInitOmega	= false,		// tracking which arguments were typed
					gotInitPi		= false,		// into our object box.
					gotInitDelta	= false;
	short			initType		= imDefault;	// Values to use if nothing specified
	double			initOmega		= 0.0,
					initPi			= 0.0,
					initDelta		= 0.0;
	
	tMutator*		me				= NIL;
	
	// Parse arguments
	while (iArgCount-- > 0) {
		switch (iArgVector->a_type) {
			case A_FLOAT:
				if (!gotInitOmega) {
					gotInitOmega = true;
					initOmega = atom_getfloat(iArgVector);
					if		(initOmega < 0.0)	initOmega = 0.0;
					else if	(1.0 < initOmega)	initOmega = 1.0;
					}
				else if (!gotInitPi) {
					gotInitPi = true;
					initPi = atom_getfloat(iArgVector);
					if		(initPi < 0.0)		initPi = 0.0;
					else if	(kMaxPi < initPi)	initPi = kMaxPi;
					}
				else if (!gotInitDelta) {
					gotInitDelta = true;
					initDelta = atom_getfloat(iArgVector);
					if		(initDelta < -1.0)	initDelta = 0.0;
					else if	(1.0 < initDelta)	initDelta = 1.0;
					}
				else goto punt_argument;					// don't need this argument
				break;
			
			case A_SYM:
				if (gotInitType) goto punt_argument;		// naughty argument
				
				gotInitType = true;
				initType = MapStrToMutationType(atom_getsym(iArgVector)->s_name);
				if (initType < 0) {
					initType = imDefault;
					goto punt_argument;
					}
				break;
			
			default:
			punt_argument:
				error("%s received spurious argument ", iSelector->s_name);
				postatom(iArgVector);
				break;
			}
		iArgVector += 1;
		}
	
	// Let Max/MSP allocate us
        me = object_alloc(gObjectClass);
	
	// Add inlets, from right to left
	floatin(me, 4);
	floatin(me, 3);
	floatin(me, 2);
	floatin(me, 1);
	
	// And outlet, main outlet accessed through me->coreObject.o_outlet.
	floatout(me);			
	
	// Set up default/initial values
	Initialize(me, initType, initOmega, initPi, initDelta, gotInitDelta);
	
	me->pendTarget	= 0.0;
	
	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	FlimSet(me, iSource)
 *
 ******************************************************************************************/

	static double CalcAbsUniform(tMutator* me, tInterval iSource)
		{ return me->params.function.uniformFunc(iSource, me->pendTarget, me->params.omega); }

	static double CalcRelUniform(tMutator* me, tInterval iSource)
		{
		double	deltaMutant	= me->params.function.uniformFunc(
												iSource - me->history.source,
												me->pendTarget - me->history.target,
												me->params.omega),
				deltaEmph	= me->params.delta;
		
		return (deltaEmph >= 0)
				? EmphasizePos(me->history.mutant, deltaMutant, 1.0 - deltaEmph)
				: EmphasizeNeg(me->history.mutant, deltaMutant, 1.0 + deltaEmph);
		}

	static Boolean MutateOrNot(tMutator* me)
		{
		double			omega = me->params.omega;
		unsigned long	thresh;
		
		switch (me->history.state) {
			case stateSource:
				thresh = CalcSourceToMutantThresh(omega, CalcOmegaPrime(omega, me->params.pi));
				break;
			case stateTarget:
				thresh = CalcMutantToMutantThresh(CalcOmegaPrime(omega, me->params.pi));
				break;
			default:
				thresh = CalcInitThresh(omega);
				break;
			}
		
		return WannaMutateStrict(thresh);
		}
	
	static double CalcAbsIrreg(tMutator* me, tInterval iSource)
		{
		if ( MutateOrNot(me) ) {
			me->history.state = stateTarget;
			return me->params.function.irregularFunc(iSource, me->pendTarget);
			}
		else {
			me->history.state = stateSource;
			return iSource;
			}
		}

	static double CalcRelIrreg(tMutator* me, tInterval iSource)
		{
		tInterval	deltaSource = iSource - me->history.source;
		double		deltaMutant = MutateOrNot(me)
									? me->params.function.irregularFunc(deltaSource,
																me->pendTarget - me->history.target)
									: deltaSource,
					deltaEmph	= me->params.delta;
				
		return (deltaEmph >= 0)
				? EmphasizePos(me->history.mutant, deltaMutant, 1.0 - deltaEmph)
				: EmphasizeNeg(me->history.mutant, deltaMutant, 1.0 + deltaEmph);
		}

void
FlimSet(
	tMutator*	me,
	double		iSource)
	
	{
	double	mutant;
		
	if (!me->params.irregular) {
		mutant = (me->params.relInterval)
					? CalcRelUniform(me, iSource)
					: CalcAbsUniform(me, iSource);
		
		}
	else {
		// Irregular mutation
		// The irregular calculations update the history.state component as a side-effect.
		mutant = (me->params.relInterval)
					? CalcRelIrreg(me, iSource)
					: CalcAbsIrreg(me, iSource);
		}
	
//	me->outOfRange = CheckRange(me, &mutant);
	
	me->history.source = iSource;
	me->history.target = me->pendTarget;
	me->history.mutant = mutant;
	
	}

/******************************************************************************************
 *
 *	FlimSource(me, iSource)
 *	FlimTarget(me, iTarget)
 *	
 *	FlimSourceInt(me, iSource)
 *
 ******************************************************************************************/

static void
FlimSource(
	tMutator*	me,
	double		iSource)
	
	{
	FlimSet(me, iSource);
	DoBang(me);
	}

static void
FlimTarget(
	tMutator*	me,
	double		iTarget)
	
	{
	me->pendTarget = iTarget;
	}

/******************************************************************************************
 *
 *	FlimSourceInt(me, iSource)
 *	FlimTargetInt(me, iTarget)
 *	FlimOmegaInt(me, iOmega)
 *	FlimPiInt(me, iPi)
 *	FlimDeltaInt(me, iDelta)
 *	
 *	Have to explicitly typecast long to double after an unannounced change in the API.-\
 *
 ******************************************************************************************/

static void FlimSourceInt(tMutator* me, long iSource)
	{ FlimSource(me, (double) iSource); }
	
static void FlimTargetInt(tMutator* me, long iTarget)
	{ FlimTarget(me, (double) iTarget); }
	
static void FlimOmegaInt(tMutator* me, long iOmega)
	{ DoOmega(me, (double) iOmega); }
	
static void FlimPiInt(tMutator* me, long iPi)
	{ DoPi(me, (double) iPi); }
	
static void FlimDeltaInt(tMutator* me, long iDelta)
	{ DoDelta(me, (double) iDelta); }
	

/******************************************************************************************
 *
 *	FlimAssist()
 *	FlimInfo()
 *	FlimVers()
 *	FlimTattle()
 *
 *	Fairly generic implementations
 *	
 *	We don't use a lot of the function parameters.
 *
 ******************************************************************************************/

void FlimAssist(tMutator* me, void* box, long iDir, long iArgNum, char oCStr[])
	{
	#pragma unused(me, box)
	
	//LitterAssist(iDir, iArgNum, strIndexFirstIn, strIndexFirstOut, oDestCStr);
        if (iDir == ASSIST_INLET) {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistIn1); break;
                case 1: sprintf (oCStr, LPAssistIn2); break;
                case 2: sprintf (oCStr, LPAssistIn3); break;
                case 3: sprintf (oCStr, LPAssistIn4); break;
                case 4: sprintf (oCStr, LPAssistIn5); break;
            }
        }
        else {
            switch(iArgNum) {
                case 0: sprintf (oCStr, LPAssistOut1); break;
                    //case 1: sprintf(s, "..."); break;
            }
            
        }
	}

void FlimInfo(tMutator* me)
	{ LitterInfo(kClassName, &me->coreObject, (method) FlimTattle); }

void
FlimTattle(
	tMutator* me)
	
	{
	
	post("%s state", kClassName);
	post("  Mutation type: %s", kLongNames[CalcMutationIndex(me)]);
	
	post("  Mutation index: %lf", me->params.omega);
	
	if (me->params.irregular) {
		post("  Clumping factor: %lf", me->params.pi);
		}
		
	if (me->params.relInterval) {
		post("  Using Relative Intervals");
		post("    Delta Emphasis (if no signal): %lf", me->params.delta);
		}
	else {
		post("  Using Absolute Intervals");
		}
	
	post("Previous Source: %lf, Target: %lf, Mutant: %lf",
		me->history.source, me->history.target, me->history.mutant);
	
	}


/******************************************************************************************
 *
 *	FlimClumpStrict(me)
 *	FlimClumpTight(me, iClumpSize)
 *	FlimClumpHard(me, iClumpSize)
 *
 ******************************************************************************************/

void FlimClumpStrict(tMutator* me)
	{ me->params.clumpLen = 0; }

void FlimClumpTight(tMutator*me , long iClumpSize)
	{ me->params.clumpLen = (iClumpSize >= 0) ? iClumpSize : 0; }

void FlimClumpHard(tMutator*me , long iClumpSize)
	{ me->params.clumpLen = (iClumpSize >= 0) ? -iClumpSize : 0; }

