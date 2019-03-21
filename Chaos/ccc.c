/*
	File:		ccc.c

	Contains:	1/f noise, using the Schuster/Procaccia deterministic (chaotic) algorithm.

	Written by:	Peter Castine

	Copyright:	© 2001-2003 Peter Castine

	Change History (most recent first):

         <5>   23–3–2006    pc      Remove unneeded references to Taus88 code.
         <4>   23–3–2006    pc      Update #includes for new LitterLib organization.
         <3>     10–1–04    pc      Update for Windows.
         <2>    7–7–2003    pc      Use new LitterInfo method (incl. gModDate, LitterAddClass, etc.)
         <1>    6–3–2003    pc      First implementation, based on lp.ccc~
*/


/******************************************************************************************
 ******************************************************************************************/

#pragma mark • Include Files

#include "LitterLib.h"
//#include "TrialPeriodUtils.h"
#include "MiscUtils.h"

#pragma mark • Constants

const char*			kClassName		= "lp.ccc";			// Class name


#define LPAssistIn1			"Bang (or float to set new seed)"
#define LPAssistOut1		"Float (1/f chaos)"

	// Indices for STR# resource
enum {
	strIndexInBang		= lpStrIndexLastStandard + 1,
	strIndexTheOutlet,
	
	strIndexInLeft		= strIndexInBang,
	strIndexOutLeft		= strIndexTheOutlet
	};


#pragma mark • Type Definitions



#pragma mark • Object Structure

typedef struct {
	t_object		coreObject;
	
	tOutletPtr	theOutlet;
	
	double		curVal;
	} objSchuster;


#pragma mark • Global Variables


#pragma mark • Function Prototypes

	// Class message functions
void*	CCCNew(double);

	// Object message functions
static void CCCBang(objSchuster*);
static void CCCFloat(objSchuster*, double);
static void CCCTattle(objSchuster*);
static void	CCCAssist(objSchuster*, void* , long , long , char*);
static void	CCCInfo(objSchuster*);


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
        
	// Standard Max initialization mantra
	c = class_new(kClassName,				// Pointer to our class definition
			(method) CCCNew,			// Instance creation function
			NIL,						// No custom deallocation function
			sizeof(objSchuster),				// Class object size
			NIL,						// No menu function
			A_DEFFLOAT,					// Optional arguments:	1. Seed
			0);		
	
	// Messages
	class_addmethod(c, (method) CCCBang,    "bang", 0);
	class_addmethod(c, (method) CCCFloat,   "float", A_FLOAT, 0);
	class_addmethod(c, (method) CCCTattle,	"dblclick",	A_CANT, 0);
	class_addmethod(c, (method) CCCTattle,	"tattle",	A_NOTHING);
	class_addmethod(c, (method) CCCAssist,	"assist",	A_CANT, 0);
	class_addmethod(c, (method) CCCInfo,	"info",		A_CANT, 0);
	//class_addmethod(c, (method) LitterVers,	"vers",		A_DEFLONG, A_DEFLONG, 0);
	
		
	// Initialize Litter Library
	//LitterInit(kClassName, 0);
        class_register(CLASS_BOX, c);
        gObjectClass = c;
        
        post("%s: %s", kClassName, LPVERSION);
        return 0;
	
	}


#pragma mark -
#pragma mark • Utilities

/******************************************************************************************
 *
 *	CheckSeed(iSeed)
 *	
 ******************************************************************************************/

static Boolean
CheckSeed(
	double iSeed)
	
	{
	if (iSeed <= 0.0 || 1.0 <= iSeed) {
		error("%s: seed must be in the range 0 < s < 1", (char*) kClassName);
		return false;
		}
	else return true;
	}


#pragma mark -
#pragma mark • Class Message Handlers

/******************************************************************************************
 *
 *	CCCNew(iSeed)
 *	
 *	Arguments: seed value, valid values are in the range 0 < s < 1.
 *
 ******************************************************************************************/

void*
CCCNew(
	double iSeed)
	
	{
	objSchuster*	me	= NIL;
	
	
	// Only one initialization parameter, handle default and invalid values
	if (iSeed == 0.0 || !CheckSeed(iSeed))
		iSeed = ULong2Unit_zo(MachineKharma());
	// Finished checking intialization parameters

	// Let Max allocate us. No inlets, just one outlet.
	me = object_alloc(gObjectClass);
	me->theOutlet = floatout(me);
	
	// Set up object components
	me->curVal = iSeed;
	
	return me;
	}

#pragma mark -
#pragma mark • Object Message Handlers

/******************************************************************************************
 *
 *	CCCBang(me)
 *
 ******************************************************************************************/

void
CCCBang(
	objSchuster* me)
	
	{
	const double kMinPink = 1.0 / 525288.0;
	
	double curVal = me->curVal;
	
	// Sanity check... due to limitations in accuracy, we can die at very small values.
	// Also, we prefer to only "nudge" the value towards chaos... 
	if (curVal <= kMinPink) {
		if (curVal == 0.0)	curVal  = kMinPink;
		else				curVal += curVal;
		}
	
		curVal = curVal * curVal + curVal;
		if (curVal >= 1.0)					// Cheaper than fmod(), and works quite nicely
			curVal -= 1.0;					// in the range of values that can occur.
	
	outlet_float(me->theOutlet, curVal);
	
	// And store for next time
	me->curVal = curVal;
	
	}

/******************************************************************************************
 *
 *	CCCFloat(me)
 *
 ******************************************************************************************/

void
CCCFloat(
	objSchuster*	me,
	double	iVal)
	
	{
	
	if ( CheckSeed(iVal) ) {
		me->curVal = iVal;
		CCCBang(me);
		}
	
	// Otherwise nothing do to. CheckSeed() will have complained to the Max window
	
	}

/******************************************************************************************
 *
 *	CCCTattle(me)
 *
 *	Post state information
 *
 ******************************************************************************************/

void
CCCTattle(
	objSchuster* me)
	
	{
	
	post("%s state", kClassName);
	post("  CurVal: %lf", me->curVal);
	
	}


/******************************************************************************************
 *
 *	CCCAssist()
 *	CCCInfo()
 *
 *	Generic Assist/Info methods
 *	
 *	Many parameters are not used.
 *
 ******************************************************************************************/

void CCCAssist(objSchuster* me, void* box, long iDir, long iArgNum, char* oCStr)
	{
	#pragma unused(me, box)
	
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

void CCCInfo(objSchuster* me)
	{ //LitterInfo(kClassName, &me->coreObject, (method) CCCTattle);
    }



