/*  armcopro.c -- co-processor interface:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include "armdefs.h"

extern unsigned ARMul_CoProInit(ARMul_State *state) ;
extern void ARMul_CoProExit(ARMul_State *state) ;
extern void ARMul_CoProAttach(ARMul_State *state, unsigned number,
                              ARMul_CPInits *init, ARMul_CPExits *exit,
                              ARMul_LDCs *ldc, ARMul_STCs *stc,
                              ARMul_MRCs *mrc, ARMul_MCRs *mcr,
                              ARMul_CDPs *cdp,
                              ARMul_CPReads *read, ARMul_CPWrites *write) ;
extern void ARMul_CoProDetach(ARMul_State *state, unsigned number) ;


/***************************************************************************\
*                            Dummy Co-processors                            *
\***************************************************************************/

static unsigned NoCoPro3R(ARMul_State *state,unsigned,ARMword) ;
static unsigned NoCoPro4R(ARMul_State *state,unsigned,ARMword,ARMword) ;
static unsigned NoCoPro4W(ARMul_State *state,unsigned,ARMword,ARMword *) ;

/***************************************************************************\
*                Define Co-Processor instruction handlers here              *
\***************************************************************************/

/* Here's ARMulator's MMU definition.  A few things to note:
1) it has eight registers, but only two are defined.
2) you can only access its registers with MCR and MRC.
3) MMU Register 0 (ID) returns 0x41440110
4) Register 1 only has 4 bits defined.  Bits 0 to 3 are unused, bit 4
controls 32/26 bit program space, bit 5 controls 32/26 bit data space,
bit 6 controls late abort timimg and bit 7 controls big/little endian.
*/

static ARMword MMUReg[8] ;

static unsigned MMUInit(ARMul_State *state)
{MMUReg[1] = state->prog32Sig << 4 |
             state->data32Sig << 5 |
             state->lateabtSig << 6 |
             state->bigendSig << 7 ;
// ARMul_ConsolePrint (state, ", MMU present") ;
 return(TRUE) ;
}

static unsigned MMUMRC(ARMul_State *state, unsigned type, ARMword instr,ARMword *value)
{
	*value = mmu_mrc(state, instr);
	return(ARMul_DONE);
}

static unsigned MMUMCR(ARMul_State *state, unsigned type, ARMword instr, ARMword value)
{
	mmu_mcr(state, instr, value);
	return(ARMul_DONE);
}


/***************************************************************************\
*         Install co-processor instruction handlers in this routine         *
\***************************************************************************/

unsigned ARMul_CoProInit(ARMul_State *state)
{register unsigned i ;

 for (i = 0 ; i < 16 ; i++) /* initialise tham all first */
    ARMul_CoProDetach(state, i) ;

 /* Install CoPro Instruction handlers here
    The format is
    ARMul_CoProAttach(state, CP Number, Init routine, Exit routine
                      LDC routine, STC routine, MRC routine, MCR routine,
                      CDP routine, Read Reg routine, Write Reg routine) ;
   */

    ARMul_CoProAttach(state, 15, MMUInit, NULL,
                      NULL, NULL, MMUMRC, MMUMCR,
                      NULL, NULL, NULL) ;


    /* No handlers below here */

    for (i = 0 ; i < 16 ; i++) /* Call all the initialisation routines */
       if (state->CPInit[i])
          (state->CPInit[i])(state) ;
    return(TRUE) ;
 }

/***************************************************************************\
*         Install co-processor finalisation routines in this routine        *
\***************************************************************************/

void ARMul_CoProExit(ARMul_State *state)
{register unsigned i ;

 for (i = 0 ; i < 16 ; i++)
    if (state->CPExit[i])
       (state->CPExit[i])(state) ;
 for (i = 0 ; i < 16 ; i++) /* Detach all handlers */
    ARMul_CoProDetach(state, i) ;
 }

/***************************************************************************\
*              Routines to hook Co-processors into ARMulator                 *
\***************************************************************************/

void ARMul_CoProAttach(ARMul_State *state, unsigned number,
                       ARMul_CPInits *init,  ARMul_CPExits *exit,
                       ARMul_LDCs *ldc,  ARMul_STCs *stc,
                       ARMul_MRCs *mrc,  ARMul_MCRs *mcr,  ARMul_CDPs *cdp,
                       ARMul_CPReads *read, ARMul_CPWrites *write)
{if (init != NULL)
    state->CPInit[number] = init ;
 if (exit != NULL)
    state->CPExit[number] = exit ;
 if (ldc != NULL)
    state->LDC[number] = ldc ;
 if (stc != NULL)
    state->STC[number] = stc ;
 if (mrc != NULL)
    state->MRC[number] = mrc ;
 if (mcr != NULL)
    state->MCR[number] = mcr ;
 if (cdp != NULL)
    state->CDP[number] = cdp ;
 if (read != NULL)
    state->CPRead[number] = read ;
 if (write != NULL)
    state->CPWrite[number] = write ;
}

void ARMul_CoProDetach(ARMul_State *state, unsigned number)
{ARMul_CoProAttach(state, number, NULL, NULL,
                   NoCoPro4R, NoCoPro4W, NoCoPro4W, NoCoPro4R,
                   NoCoPro3R, NULL, NULL) ;
 state->CPInit[number] = NULL ;
 state->CPExit[number] = NULL ;
 state->CPRead[number] = NULL ;
 state->CPWrite[number] = NULL ;
}

/***************************************************************************\
*         There is no CoPro around, so Undefined Instruction trap           *
\***************************************************************************/

static unsigned NoCoPro3R(ARMul_State *state,unsigned a,ARMword b)
{return(ARMul_CANT) ;}

static unsigned NoCoPro4R(ARMul_State *state, unsigned a,ARMword b,ARMword c)
{return(ARMul_CANT) ;}

static unsigned NoCoPro4W(ARMul_State *state, unsigned a,ARMword b,ARMword *c)
{return(ARMul_CANT) ;}
