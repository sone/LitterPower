/*	File:		RNGChi2.h	Contains:	<contents>	Written by:	Peter Castine	Copyright:	<copyright>	Change History (most recent first):         <1>   30󕿜006    pc      Initial check in.*//*	File:		RNGChi2.h	Contains:	Header file for RNGChi2.c.	Written by:	Peter Castine	Copyright:	� 2001-06 Peter Castine	Change History (most recent first):*//******************************************************************************************	Previous history:		24-Mar-06:	Spun off from LitterLib ******************************************************************************************/#pragma once#ifndef __RNGCHI2_H__#define __RNGCHI2_H__#pragma mark � Include Files#ifndef __TAUS88_H__	#include "Taus88.h"#endif#ifndef __MISCUTILS_H__	#include "MiscUtils.h"#endif#ifndef __RNGGAMMA_H__	#include "RNGGamma.h"#endif#pragma mark � Constantsenum chi2Alg {	algChi2Undef	= -1,	algChi2Const0,	algChi2Dir,	algChi2Rej	};#pragma mark � Type Definitionstypedef enum chi2Alg eChi2Alg;#pragma mark � Global Variables#pragma mark � Function Prototypesstatic inline eChi2Alg	Chi2RecommendAlg(UInt32);double	GenChi2Dir		(UInt32, tRandomFunc, void*);double	GenChi2DirTaus88(UInt32, tTaus88Data*);static inline double	CalcChi2RejGamma(UInt32);double	GenChi2Rej		(UInt32, double, tRandomFunc, void*);double	GenChi2RejTaus88(UInt32, double, tTaus88Data*);#pragma mark -#pragma mark � Inline Functions & Macrosstatic inline eChi2Alg Chi2RecommendAlg(UInt32 iDoF)		{		if		(iDoF == 0)		return algChi2Const0;		else if (iDoF <= 26)	return algChi2Dir;		else					return algChi2Rej;		}static inline double CalcChi2RejGamma(UInt32 iDof)		{ return CalcErlangRejGamma(iDof >> 1); }#endif			// __RNGCHI2_H__