/*  armemu.c -- Main instruction emulation:  ARM7 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
    Modifications to add arch. v4 support by <jsmith@cygnus.com>.
 
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
#include "armemu.h"

static ARMword GetDPRegRHS(ARMul_State *state, ARMword instr) ;
static ARMword GetDPSRegRHS(ARMul_State *state, ARMword instr) ;
static void WriteR15(ARMul_State *state, ARMword src) ;
static void WriteSR15(ARMul_State *state, ARMword src) ;
static ARMword GetLSRegRHS(ARMul_State *state, ARMword instr) ;
static ARMword GetLS7RHS(ARMul_State *state, ARMword instr) ;
static unsigned LoadWord(ARMul_State *state, ARMword instr, ARMword address) ;
static unsigned LoadHalfWord(ARMul_State *state, ARMword instr, ARMword address,int signextend) ;
static unsigned LoadByte(ARMul_State *state, ARMword instr, ARMword address,int signextend) ;
static unsigned StoreWord(ARMul_State *state, ARMword instr, ARMword address) ;
static unsigned StoreHalfWord(ARMul_State *state, ARMword instr, ARMword address) ;
static unsigned StoreByte(ARMul_State *state, ARMword instr, ARMword address) ;
static void LoadMult(ARMul_State *state, ARMword address, ARMword instr, ARMword WBBase) ;
static void StoreMult(ARMul_State *state, ARMword address, ARMword instr, ARMword WBBase) ;
static void LoadSMult(ARMul_State *state, ARMword address, ARMword instr, ARMword WBBase) ;
static void StoreSMult(ARMul_State *state, ARMword address, ARMword instr, ARMword WBBase) ;
static unsigned Multiply64(ARMul_State *state, ARMword instr,int signextend,int scc) ;
static unsigned MultiplyAdd64(ARMul_State *state, ARMword instr,int signextend,int scc) ;

#define LUNSIGNED (0)   /* unsigned operation */
#define LSIGNED   (1)   /* signed operation */
#define LDEFAULT  (0)   /* default : do nothing */
#define LSCC      (1)   /* set condition codes on result */

extern int stop_simulator;

#ifdef NEED_UI_LOOP_HOOK
/* How often to run the ui_loop update, when in use */
#define UI_LOOP_POLL_INTERVAL 0x32000

/* Counter for the ui_loop_hook update */
static long ui_loop_hook_counter = UI_LOOP_POLL_INTERVAL;

/* Actual hook to call to run through gdb's gui event loop */
extern int (*ui_loop_hook) (int);
#endif /* NEED_UI_LOOP_HOOK */

/***************************************************************************\
*               short-hand macros for LDR/STR                               *
\***************************************************************************/

/* store post decrement writeback */
#define SHDOWNWB()                                      \
  lhs = LHS ;                                           \
  if (StoreHalfWord(state, instr, lhs))                 \
     LSBase = lhs - GetLS7RHS(state, instr) ;

/* store post increment writeback */
#define SHUPWB()                                        \
  lhs = LHS ;                                           \
  if (StoreHalfWord(state, instr, lhs))                 \
     LSBase = lhs + GetLS7RHS(state, instr) ;

/* store pre decrement */
#define SHPREDOWN()                                     \
  (void)StoreHalfWord(state, instr, LHS - GetLS7RHS(state, instr)) ;

/* store pre decrement writeback */
#define SHPREDOWNWB()                                   \
  temp = LHS - GetLS7RHS(state, instr) ;                \
  if (StoreHalfWord(state, instr, temp))                \
     LSBase = temp ;

/* store pre increment */
#define SHPREUP()                                       \
  (void)StoreHalfWord(state, instr, LHS + GetLS7RHS(state, instr)) ;  

/* store pre increment writeback */
#define SHPREUPWB()                                     \
  temp = LHS + GetLS7RHS(state, instr) ;                \
  if (StoreHalfWord(state, instr, temp))                \
     LSBase = temp ;

/* load post decrement writeback */
#define LHPOSTDOWN()                                    \
{                                                       \
  int done = 1 ;                                        \
  lhs = LHS ;                                           \
  switch (BITS(5,6)) {                                  \
    case 1: /* H */                                     \
      if (LoadHalfWord(state,instr,lhs,LUNSIGNED))      \
         LSBase = lhs - GetLS7RHS(state,instr) ;        \
      break ;                                           \
    case 2: /* SB */                                    \
      if (LoadByte(state,instr,lhs,LSIGNED))            \
         LSBase = lhs - GetLS7RHS(state,instr) ;        \
      break ;                                           \
    case 3: /* SH */                                    \
      if (LoadHalfWord(state,instr,lhs,LSIGNED))        \
         LSBase = lhs - GetLS7RHS(state,instr) ;        \
      break ;                                           \
    case 0: /* SWP handled elsewhere */                 \
    default:                                            \
      done = 0 ;                                        \
      break ;                                           \
    }                                                   \
  if (done)                                             \
     return ;                                           \
}

/* load post increment writeback */
#define LHPOSTUP()                                      \
{                                                       \
  int done = 1 ;                                        \
  lhs = LHS ;                                           \
  switch (BITS(5,6)) {                                  \
    case 1: /* H */                                     \
      if (LoadHalfWord(state,instr,lhs,LUNSIGNED))      \
         LSBase = lhs + GetLS7RHS(state,instr) ;        \
      break ;                                           \
    case 2: /* SB */                                    \
      if (LoadByte(state,instr,lhs,LSIGNED))            \
         LSBase = lhs + GetLS7RHS(state,instr) ;        \
      break ;                                           \
    case 3: /* SH */                                    \
      if (LoadHalfWord(state,instr,lhs,LSIGNED))        \
         LSBase = lhs + GetLS7RHS(state,instr) ;        \
      break ;                                           \
    case 0: /* SWP handled elsewhere */                 \
    default:                                            \
      done = 0 ;                                        \
      break ;                                           \
    }                                                   \
  if (done)                                             \
     return ;                                           \
}

/* load pre decrement */
#define LHPREDOWN()                                     \
{                                                       \
  int done = 1 ;                                        \
  temp = LHS - GetLS7RHS(state,instr) ;                 \
  switch (BITS(5,6)) {                                  \
    case 1: /* H */                                     \
      (void)LoadHalfWord(state,instr,temp,LUNSIGNED) ;  \
      break ;                                           \
    case 2: /* SB */                                    \
      (void)LoadByte(state,instr,temp,LSIGNED) ;        \
      break ;                                           \
    case 3: /* SH */                                    \
      (void)LoadHalfWord(state,instr,temp,LSIGNED) ;    \
      break ;                                           \
    case 0: /* SWP handled elsewhere */                 \
    default:                                            \
      done = 0 ;                                        \
      break ;                                           \
    }                                                   \
  if (done)                                             \
     return ;                                           \
}

/* load pre decrement writeback */
#define LHPREDOWNWB()                                   \
{                                                       \
  int done = 1 ;                                        \
  temp = LHS - GetLS7RHS(state, instr) ;                \
  switch (BITS(5,6)) {                                  \
    case 1: /* H */                                     \
      if (LoadHalfWord(state,instr,temp,LUNSIGNED))     \
         LSBase = temp ;                                \
      break ;                                           \
    case 2: /* SB */                                    \
      if (LoadByte(state,instr,temp,LSIGNED))           \
         LSBase = temp ;                                \
      break ;                                           \
    case 3: /* SH */                                    \
      if (LoadHalfWord(state,instr,temp,LSIGNED))       \
         LSBase = temp ;                                \
      break ;                                           \
    case 0: /* SWP handled elsewhere */                 \
    default:                                            \
      done = 0 ;                                        \
      break ;                                           \
    }                                                   \
  if (done)                                             \
     return ;                                           \
}

/* load pre increment */
#define LHPREUP()                                       \
{                                                       \
  int done = 1 ;                                        \
  temp = LHS + GetLS7RHS(state,instr) ;                 \
  switch (BITS(5,6)) {                                  \
    case 1: /* H */                                     \
      (void)LoadHalfWord(state,instr,temp,LUNSIGNED) ;  \
      break ;                                           \
    case 2: /* SB */                                    \
      (void)LoadByte(state,instr,temp,LSIGNED) ;        \
      break ;                                           \
    case 3: /* SH */                                    \
      (void)LoadHalfWord(state,instr,temp,LSIGNED) ;    \
      break ;                                           \
    case 0: /* SWP handled elsewhere */                 \
    default:                                            \
      done = 0 ;                                        \
      break ;                                           \
    }                                                   \
  if (done)                                             \
     return ;                                           \
}

/* load pre increment writeback */
#define LHPREUPWB()                                     \
{                                                       \
  int done = 1 ;                                        \
  temp = LHS + GetLS7RHS(state, instr) ;                \
  switch (BITS(5,6)) {                                  \
    case 1: /* H */                                     \
      if (LoadHalfWord(state,instr,temp,LUNSIGNED))     \
         LSBase = temp ;                                \
      break ;                                           \
    case 2: /* SB */                                    \
      if (LoadByte(state,instr,temp,LSIGNED))           \
         LSBase = temp ;                                \
      break ;                                           \
    case 3: /* SH */                                    \
      if (LoadHalfWord(state,instr,temp,LSIGNED))       \
         LSBase = temp ;                                \
      break ;                                           \
    case 0: /* SWP handled elsewhere */                 \
    default:                                            \
      done = 0 ;                                        \
      break ;                                           \
    }                                                   \
  if (done)                                             \
     return ;                                           \
}

ARMword dest; /* almost the DestBus */
ARMword temp; /* ubiquitous third hand */
ARMword pc ; /* the address of the current instruction */
ARMword lhs, rhs ; /* almost the ABus and BBus */
ARMword decoded, loaded ; /* instruction pipeline */

void op0x00(register ARMul_State *state, register ARMword instr)
{
#ifdef MODET
	if (BITS(4,11) == 0xB) {
		/* STRH register offset, no write-back, down, post indexed */
		SHDOWNWB() ;
		return ;
	}
	/* TODO: CHECK: should 0xD and 0xF generate undefined intruction aborts? */
#endif
	if (BITS(4,7) == 9) { /* MUL */
		rhs = state->Reg[MULRHSReg] ;
		if (MULLHSReg == MULDESTReg) {
			UNDEF_MULDestEQOp1 ;
			state->Reg[MULDESTReg] = 0 ;
		}
		else if (MULDESTReg != 15)
			state->Reg[MULDESTReg] = state->Reg[MULLHSReg] * rhs ;
		else {
			UNDEF_MULPCDest ;
		}
		for (dest = 0, temp = 0 ; dest < 32 ; dest++)
			if (rhs & (1L << dest))
				temp = dest ; /* mult takes this many/2 I cycles */
		ARMul_Icycles(state,ARMul_MultTable[temp],0L) ;
	}
	else { /* AND reg */
		rhs = DPRegRHS ;
		dest = LHS & rhs ;
		WRITEDEST(dest) ;
	}
	return ;
}

void op0x01(register ARMul_State *state, register ARMword instr)
{ /* ANDS reg and MULS */
#ifdef MODET
	if ((BITS(4,11) & 0xF9) == 0x9) {
		/* LDR register offset, no write-back, down, post indexed */
		LHPOSTDOWN() ;
		/* fall through to rest of decoding */
	}
#endif
	if (BITS(4,7) == 9) { /* MULS */
		rhs = state->Reg[MULRHSReg] ;
		if (MULLHSReg == MULDESTReg) {
			UNDEF_MULDestEQOp1 ;
			state->Reg[MULDESTReg] = 0 ;
			CLEARN ;
			SETZ ;
		}
		else if (MULDESTReg != 15) {
			dest = state->Reg[MULLHSReg] * rhs ;
			ARMul_NegZero(state,dest) ;
			state->Reg[MULDESTReg] = dest ;
		}
                else {
			UNDEF_MULPCDest ;
		}
		for (dest = 0, temp = 0 ; dest < 32 ; dest++)
			if (rhs & (1L << dest))
				temp = dest ; /* mult takes this many/2 I cycles */
		ARMul_Icycles(state,ARMul_MultTable[temp],0L) ;
	}
	else { /* ANDS reg */
		rhs = DPSRegRHS ;
		dest = LHS & rhs ;
		WRITESDEST(dest) ;
	}
	return;
}

void op0x02(register ARMul_State *state, register ARMword instr)
{ /* EOR reg and MLA */
#ifdef MODET
             if (BITS(4,11) == 0xB) {
               /* STRH register offset, write-back, down, post indexed */
               SHDOWNWB() ;
               return ;
               }
#endif
             if (BITS(4,7) == 9) { /* MLA */
                rhs = state->Reg[MULRHSReg] ;
                if (MULLHSReg == MULDESTReg) {
                   UNDEF_MULDestEQOp1 ;
                   state->Reg[MULDESTReg] = state->Reg[MULACCReg] ;
                   }
                else if (MULDESTReg != 15)
                   state->Reg[MULDESTReg] = state->Reg[MULLHSReg] * rhs + state->Reg[MULACCReg] ;
                else {
                   UNDEF_MULPCDest ;
                   }
                for (dest = 0, temp = 0 ; dest < 32 ; dest++)
                   if (rhs & (1L << dest))
                      temp = dest ; /* mult takes this many/2 I cycles */
                ARMul_Icycles(state,ARMul_MultTable[temp],0L) ;
                }
             else {
                rhs = DPRegRHS ;
                dest = LHS ^ rhs ;
                WRITEDEST(dest) ;
                }
	return;
}

void op0x03(register ARMul_State *state, register ARMword instr)
{ /* EORS reg and MLAS */
#ifdef MODET
             if ((BITS(4,11) & 0xF9) == 0x9) {
               /* LDR register offset, write-back, down, post-indexed */
               LHPOSTDOWN() ;
               /* fall through to rest of the decoding */
               }
#endif
             if (BITS(4,7) == 9) { /* MLAS */
                rhs = state->Reg[MULRHSReg] ;
                if (MULLHSReg == MULDESTReg) {
                   UNDEF_MULDestEQOp1 ;
                   dest = state->Reg[MULACCReg] ;
                   ARMul_NegZero(state,dest) ;
                   state->Reg[MULDESTReg] = dest ;
                   }
                else if (MULDESTReg != 15) {
                   dest = state->Reg[MULLHSReg] * rhs + state->Reg[MULACCReg] ;
                   ARMul_NegZero(state,dest) ;
                   state->Reg[MULDESTReg] = dest ;
                   }
                else {
                   UNDEF_MULPCDest ;
                   }
                for (dest = 0, temp = 0 ; dest < 32 ; dest++)
                   if (rhs & (1L << dest))
                      temp = dest ; /* mult takes this many/2 I cycles */
                ARMul_Icycles(state,ARMul_MultTable[temp],0L) ;
                }
             else { /* EORS Reg */
                rhs = DPSRegRHS ;
                dest = LHS ^ rhs ;
                WRITESDEST(dest) ;
                }
	return;
}
void op0x04(register ARMul_State *state, register ARMword instr)
{ /* SUB reg */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, no write-back, down, post indexed */
               SHDOWNWB() ;
               return ;
               }
#endif
             rhs = DPRegRHS;
             dest = LHS - rhs ;
             WRITEDEST(dest) ;
             return ;
}

void op0x05(register ARMul_State *state, register ARMword instr)
{ /* SUBS reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, no write-back, down, post indexed */
               LHPOSTDOWN() ;
               /* fall through to the rest of the instruction decoding */
               }
#endif
             lhs = LHS ;
             rhs = DPRegRHS ;
             dest = lhs - rhs ;
             if ((lhs >= rhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,lhs,rhs,dest) ;
                ARMul_SubOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x06(register ARMul_State *state, register ARMword instr)
{ /* RSB reg */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, write-back, down, post indexed */
               SHDOWNWB() ;
               return ;
               }
#endif
             rhs = DPRegRHS ;
             dest = rhs - LHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x07(register ARMul_State *state, register ARMword instr)
{ /* RSBS reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, write-back, down, post indexed */
               LHPOSTDOWN() ;
               /* fall through to remainder of instruction decoding */
               }
#endif
             lhs = LHS ;
             rhs = DPRegRHS ;
             dest = rhs - lhs ;
             if ((rhs >= lhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,rhs,lhs,dest) ;
                ARMul_SubOverflow(state,rhs,lhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}
void op0x08(register ARMul_State *state, register ARMword instr)
{ /* ADD reg */
#ifdef MODET
             if (BITS(4,11) == 0xB) {
               /* STRH register offset, no write-back, up, post indexed */
               SHUPWB() ;
               return ;
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32 = 64 */
               ARMul_Icycles(state,Multiply64(state,instr,LUNSIGNED,LDEFAULT),0L) ;
               return ;
               }
#endif
             rhs = DPRegRHS ;
             dest = LHS + rhs ;
             WRITEDEST(dest) ;
             return ;
}

void op0x09(register ARMul_State *state, register ARMword instr)
{ /* ADDS reg */
#ifdef MODET
             if ((BITS(4,11) & 0xF9) == 0x9) {
               /* LDR register offset, no write-back, up, post indexed */
               LHPOSTUP() ;
               /* fall through to remaining instruction decoding */
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32=64 */
               ARMul_Icycles(state,Multiply64(state,instr,LUNSIGNED,LSCC),0L) ;
               return ;
               }
#endif
             lhs = LHS ;
             rhs = DPRegRHS ;
             dest = lhs + rhs ;
             ASSIGNZ(dest==0) ;
             if ((lhs | rhs) >> 30) { /* possible C,V,N to set */
                ASSIGNN(NEG(dest)) ;
                ARMul_AddCarry(state,lhs,rhs,dest) ;
                ARMul_AddOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARN ;
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x0a(register ARMul_State *state, register ARMword instr)
{ /* ADC reg */
#ifdef MODET
             if (BITS(4,11) == 0xB) {
               /* STRH register offset, write-back, up, post-indexed */
               SHUPWB() ;
               return ;
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32=64 */
               ARMul_Icycles(state,MultiplyAdd64(state,instr,LUNSIGNED,LDEFAULT),0L) ;
               return ;
               }
#endif
             rhs = DPRegRHS ;
             dest = LHS + rhs + CFLAG ;
             WRITEDEST(dest) ;
             return ;
}

void op0x0b(register ARMul_State *state, register ARMword instr)
{ /* ADCS reg */
#ifdef MODET
             if ((BITS(4,11) & 0xF9) == 0x9) {
               /* LDR register offset, write-back, up, post indexed */
               LHPOSTUP() ;
               /* fall through to remaining instruction decoding */
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32=64 */
               ARMul_Icycles(state,MultiplyAdd64(state,instr,LUNSIGNED,LSCC),0L) ;
               return ;
               }
#endif
             lhs = LHS ;
             rhs = DPRegRHS ;
             dest = lhs + rhs + CFLAG ;
             ASSIGNZ(dest==0) ;
             if ((lhs | rhs) >> 30) { /* possible C,V,N to set */
                ASSIGNN(NEG(dest)) ;
                ARMul_AddCarry(state,lhs,rhs,dest) ;
                ARMul_AddOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARN ;
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x0c(register ARMul_State *state, register ARMword instr)
{ /* SBC reg */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, no write-back, up post indexed */
               SHUPWB() ;
               return ;
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32=64 */
               ARMul_Icycles(state,Multiply64(state,instr,LSIGNED,LDEFAULT),0L) ;
               return ;
               }
#endif
             rhs = DPRegRHS ;
             dest = LHS - rhs - !CFLAG ;
             WRITEDEST(dest) ;
             return ;
}

void op0x0d(register ARMul_State *state, register ARMword instr)
{ /* SBCS reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, no write-back, up, post indexed */
               LHPOSTUP() ;
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32=64 */
               ARMul_Icycles(state,Multiply64(state,instr,LSIGNED,LSCC),0L) ;
               return ;
               }
#endif
             lhs = LHS ;
             rhs = DPRegRHS ;
             dest = lhs - rhs - !CFLAG ;
             if ((lhs >= rhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,lhs,rhs,dest) ;
                ARMul_SubOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x0e(register ARMul_State *state, register ARMword instr)
{ /* RSC reg */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, write-back, up, post indexed */
               SHUPWB() ;
               return ;
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32=64 */
               ARMul_Icycles(state,MultiplyAdd64(state,instr,LSIGNED,LDEFAULT),0L) ;
               return ;
               }
#endif
             rhs = DPRegRHS ;
             dest = rhs - LHS - !CFLAG ;
             WRITEDEST(dest) ;
             return ;
}

void op0x0f(register ARMul_State *state, register ARMword instr)
{ /* RSCS reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, write-back, up, post indexed */
               LHPOSTUP() ;
               /* fall through to remaining instruction decoding */
               }
#endif
#ifdef MODET
             if (BITS(4,7) == 0x9) { /* MULL */
               /* 32x32=64 */
               ARMul_Icycles(state,MultiplyAdd64(state,instr,LSIGNED,LSCC),0L) ;
               return ;
               }
#endif
             lhs = LHS ;
             rhs = DPRegRHS ;
             dest = rhs - lhs - !CFLAG ;
             if ((rhs >= lhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,rhs,lhs,dest) ;
                ARMul_SubOverflow(state,rhs,lhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x10(register ARMul_State *state, register ARMword instr)
{ /* TST reg and MRS CPSR and SWP word */
#ifdef MODET
             if (BITS(4,11) == 0xB) {
               /* STRH register offset, no write-back, down, pre indexed */
               SHPREDOWN() ;
               return ;
               }
#endif
             if (BITS(4,11) == 9) { /* SWP */
                UNDEF_SWPPC ;
                temp = LHS ;
                BUSUSEDINCPCS ;
#ifndef MODE32
                if (VECTORACCESS(temp) || ADDREXCEPT(temp)) {
                   INTERNALABORT(temp) ;
                   (void)ARMul_LoadWordN(state,temp) ;
                   (void)ARMul_LoadWordN(state,temp) ;
                   }
                else
#endif
                dest = ARMul_SwapWord(state,temp,state->Reg[RHSReg]) ;
                if (temp & 3)
                    DEST = ARMul_Align(state,temp,dest) ;
                else
                    DEST = dest ;
                if (state->abortSig || state->Aborted) {
                   TAKEABORT ;
                   }
                }
             else if ((BITS(0,11)==0) && (LHSReg==15)) { /* MRS CPSR */
                UNDEF_MRSPC ;
                DEST = ECC | EINT | EMODE ;
                }
             else {
                UNDEF_Test ;
                }
             return ;
}

void op0x11(register ARMul_State *state, register ARMword instr)
{ /* TSTP reg */
#ifdef MODET
             if ((BITS(4,11) & 0xF9) == 0x9) {
               /* LDR register offset, no write-back, down, pre indexed */
               LHPREDOWN() ;
               /* continue with remaining instruction decode */
               }
#endif
             if (DESTReg == 15) { /* TSTP reg */
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                rhs = DPRegRHS ;
                temp = LHS & rhs ;
                SETR15PSR(temp) ;
#endif
                }
             else { /* TST reg */
                rhs = DPSRegRHS ;
                dest = LHS & rhs ;
                ARMul_NegZero(state,dest) ;
                }
             return ;
}

void op0x12(register ARMul_State *state, register ARMword instr)
{ /* TEQ reg and MSR reg to CPSR (ARM6) */
#ifdef MODET
             if (BITS(4,11) == 0xB) {
               /* STRH register offset, write-back, down, pre indexed */
               SHPREDOWNWB() ;
               return ;
               }
#endif
#ifdef MODET
             if (BITS(4,27)==0x12FFF1) { /* BX */
               /* Branch to the address in RHSReg. If bit0 of
                  destination address is 1 then switch to Thumb mode: */
               ARMword addr = state->Reg[RHSReg];
	       
	       /* If we read the PC then the bottom bit is clear */
	       if (RHSReg == 15) addr &= ~1;
	       
	       /* Enable this for a helpful bit of debugging when
		  GDB is not yet fully working... 
	       fprintf (stderr, "BX at %x to %x (go %s)\n",
			state->Reg[15], addr, (addr & 1) ? "thumb": "arm" ); */

               if (addr & (1 << 0)) { /* Thumb bit */
                 SETT;
                 state->Reg[15] = addr & 0xfffffffe;
                 /* NOTE: The other CPSR flag setting blocks do not
                    seem to update the state->Cpsr state, but just do
                    the explicit flag. The copy from the seperate
                    flags to the register must happen later. */
                 FLUSHPIPE;
                 } else {
                 CLEART;
                 state->Reg[15] = addr & 0xfffffffc;
                 FLUSHPIPE;
                 }
               }
#endif
             if (DESTReg==15 && BITS(17,18)==0) { /* MSR reg to CPSR */
                UNDEF_MSRPC ;
                temp = DPRegRHS ;
                   ARMul_FixCPSR(state,instr,temp) ;
                }
             else {
                UNDEF_Test ;
                }
             return ;
}

void op0x13(register ARMul_State *state, register ARMword instr)
{ /* TEQP reg */
#ifdef MODET
             if ((BITS(4,11) & 0xF9) == 0x9) {
               /* LDR register offset, write-back, down, pre indexed */
               LHPREDOWNWB() ;
               /* continue with remaining instruction decode */
               }
#endif
             if (DESTReg == 15) { /* TEQP reg */
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                rhs = DPRegRHS ;
                temp = LHS ^ rhs ;
                SETR15PSR(temp) ;
#endif
                }
             else { /* TEQ Reg */
                rhs = DPSRegRHS ;
                dest = LHS ^ rhs ;
                ARMul_NegZero(state,dest) ;
                }
             return ;
}

void op0x14(register ARMul_State *state, register ARMword instr)
{ /* CMP reg and MRS SPSR and SWP byte */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, no write-back, down, pre indexed */
               SHPREDOWN() ;
               return ;
               }
#endif
             if (BITS(4,11) == 9) { /* SWP */
                UNDEF_SWPPC ;
                temp = LHS ;
                BUSUSEDINCPCS ;
#ifndef MODE32
                if (VECTORACCESS(temp) || ADDREXCEPT(temp)) {
                   INTERNALABORT(temp) ;
                   (void)ARMul_LoadByte(state,temp) ;
                   (void)ARMul_LoadByte(state,temp) ;
                   }
                else
#endif
                DEST = ARMul_SwapByte(state,temp,state->Reg[RHSReg]) ;
                if (state->abortSig || state->Aborted) {
                   TAKEABORT ;
                   }
                }
             else if ((BITS(0,11)==0) && (LHSReg==15)) { /* MRS SPSR */
                UNDEF_MRSPC ;
                DEST = GETSPSR(state->Bank) ;
                }
             else {
                UNDEF_Test ;
                }
             return ;
}

void op0x15(register ARMul_State *state, register ARMword instr)
{ /* CMPP reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, no write-back, down, pre indexed */
               LHPREDOWN() ;
               /* continue with remaining instruction decode */
               }
#endif
             if (DESTReg == 15) { /* CMPP reg */
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                rhs = DPRegRHS ;
                temp = LHS - rhs ;
                SETR15PSR(temp) ;
#endif
                }
             else { /* CMP reg */
                lhs = LHS ;
                rhs = DPRegRHS ;
                dest = lhs - rhs ;
                ARMul_NegZero(state,dest) ;
                if ((lhs >= rhs) || ((rhs | lhs) >> 31)) {
                   ARMul_SubCarry(state,lhs,rhs,dest) ;
                   ARMul_SubOverflow(state,lhs,rhs,dest) ;
                   }
                else {
                   CLEARC ;
                   CLEARV ;
                   }
                }
             return ;
}

void op0x16(register ARMul_State *state, register ARMword instr)
{ /* CMN reg and MSR reg to SPSR */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, write-back, down, pre indexed */
               SHPREDOWNWB() ;
               return ;
               }
#endif
             if (DESTReg==15 && BITS(17,18)==0) { /* MSR */
                UNDEF_MSRPC ;
                ARMul_FixSPSR(state,instr,DPRegRHS);
                }
             else {
                UNDEF_Test ;
                }
             return ;
}

void op0x17(register ARMul_State *state, register ARMword instr)
{ /* CMNP reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, write-back, down, pre indexed */
               LHPREDOWNWB() ;
               /* continue with remaining instruction decoding */
               }
#endif
             if (DESTReg == 15) {
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                rhs = DPRegRHS ;
                temp = LHS + rhs ;
                SETR15PSR(temp) ;
#endif
                return ;
                }
             else { /* CMN reg */
                lhs = LHS ;
                rhs = DPRegRHS ;
                dest = lhs + rhs ;
                ASSIGNZ(dest==0) ;
                if ((lhs | rhs) >> 30) { /* possible C,V,N to set */
                   ASSIGNN(NEG(dest)) ;
                   ARMul_AddCarry(state,lhs,rhs,dest) ;
                   ARMul_AddOverflow(state,lhs,rhs,dest) ;
                   }
                else {
                   CLEARN ;
                   CLEARC ;
                   CLEARV ;
                   }
                }
             return ;
}

void op0x18(register ARMul_State *state, register ARMword instr)
{ /* ORR reg */
#ifdef MODET
             if (BITS(4,11) == 0xB) {
               /* STRH register offset, no write-back, up, pre indexed */
               SHPREUP() ;
               return ;
               }
#endif
             rhs = DPRegRHS ;
             dest = LHS | rhs ;
             WRITEDEST(dest) ;
             return ;
}

void op0x19(register ARMul_State *state, register ARMword instr)
{ /* ORRS reg */
#ifdef MODET
             if ((BITS(4,11) & 0xF9) == 0x9) {
               /* LDR register offset, no write-back, up, pre indexed */
               LHPREUP() ;
               /* continue with remaining instruction decoding */
               }
#endif
             rhs = DPSRegRHS ;
             dest = LHS | rhs ;
             WRITESDEST(dest) ;
             return ;
}

void op0x1a(register ARMul_State *state, register ARMword instr)
{ /* MOV reg */
#ifdef MODET
             if (BITS(4,11) == 0xB) {
               /* STRH register offset, write-back, up, pre indexed */
               SHPREUPWB() ;
               return ;
               }
#endif
             dest = DPRegRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x1b(register ARMul_State *state, register ARMword instr)
{ /* MOVS reg */
#ifdef MODET
             if ((BITS(4,11) & 0xF9) == 0x9) {
               /* LDR register offset, write-back, up, pre indexed */
               LHPREUPWB() ;
               /* continue with remaining instruction decoding */
               }
#endif
             dest = DPSRegRHS ;
             WRITESDEST(dest) ;
             return ;
}

void op0x1c(register ARMul_State *state, register ARMword instr)
{ /* BIC reg */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, no write-back, up, pre indexed */
               SHPREUP() ;
               return ;
               }
#endif
             rhs = DPRegRHS ;
             dest = LHS & ~rhs ;
             WRITEDEST(dest) ;
             return ;
}

void op0x1d(register ARMul_State *state, register ARMword instr)
{ /* BICS reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, no write-back, up, pre indexed */
               LHPREUP() ;
               /* continue with instruction decoding */
               }
#endif
             rhs = DPSRegRHS ;
             dest = LHS & ~rhs ;
             WRITESDEST(dest) ;
             return ;
}

void op0x1e(register ARMul_State *state, register ARMword instr)
{ /* MVN reg */
#ifdef MODET
             if (BITS(4,7) == 0xB) {
               /* STRH immediate offset, write-back, up, pre indexed */
               SHPREUPWB() ;
               return ;
               }
#endif
             dest = ~DPRegRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x1f(register ARMul_State *state, register ARMword instr)
{ /* MVNS reg */
#ifdef MODET
             if ((BITS(4,7) & 0x9) == 0x9) {
               /* LDR immediate offset, write-back, up, pre indexed */
               LHPREUPWB() ;
               /* continue instruction decoding */
               }
#endif
             dest = ~DPSRegRHS ;
             WRITESDEST(dest) ;
             return ;
}

/***************************************************************************\
*                Data Processing Immediate RHS Instructions                 *
\***************************************************************************/

void op0x20(register ARMul_State *state, register ARMword instr)
{ /* AND immed */
             dest = LHS & DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x21(register ARMul_State *state, register ARMword instr)
{ /* ANDS immed */
             DPSImmRHS ;
             dest = LHS & rhs ;
             WRITESDEST(dest) ;
             return ;
}

void op0x22(register ARMul_State *state, register ARMword instr)
{ /* EOR immed */
             dest = LHS ^ DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x23(register ARMul_State *state, register ARMword instr)
{ /* EORS immed */
             DPSImmRHS ;
             dest = LHS ^ rhs ;
             WRITESDEST(dest) ;
             return ;
}

void op0x24(register ARMul_State *state, register ARMword instr)
{/* SUB immed */
             dest = LHS - DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x25(register ARMul_State *state, register ARMword instr)
{ /* SUBS immed */
             lhs = LHS ;
             rhs = DPImmRHS ;
             dest = lhs - rhs ;
             if ((lhs >= rhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,lhs,rhs,dest) ;
                ARMul_SubOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x26(register ARMul_State *state, register ARMword instr)
{ /* RSB immed */
             dest = DPImmRHS - LHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x27(register ARMul_State *state, register ARMword instr)
{ /* RSBS immed */
             lhs = LHS ;
             rhs = DPImmRHS ;
             dest = rhs - lhs ;
             if ((rhs >= lhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,rhs,lhs,dest) ;
                ARMul_SubOverflow(state,rhs,lhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x28(register ARMul_State *state, register ARMword instr)
{ /* ADD immed */
             dest = LHS + DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x29(register ARMul_State *state, register ARMword instr)
{ /* ADDS immed */
             lhs = LHS ;
             rhs = DPImmRHS ;
             dest = lhs + rhs ;
             ASSIGNZ(dest==0) ;
             if ((lhs | rhs) >> 30) { /* possible C,V,N to set */
                ASSIGNN(NEG(dest)) ;
                ARMul_AddCarry(state,lhs,rhs,dest) ;
                ARMul_AddOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARN ;
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x2a(register ARMul_State *state, register ARMword instr)
{ /* ADC immed */
             dest = LHS + DPImmRHS + CFLAG ;
             WRITEDEST(dest) ;
             return ;
}

void op0x2b(register ARMul_State *state, register ARMword instr)
{ /* ADCS immed */
             lhs = LHS ;
             rhs = DPImmRHS ;
             dest = lhs + rhs + CFLAG ;
             ASSIGNZ(dest==0) ;
             if ((lhs | rhs) >> 30) { /* possible C,V,N to set */
                ASSIGNN(NEG(dest)) ;
                ARMul_AddCarry(state,lhs,rhs,dest) ;
                ARMul_AddOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARN ;
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x2c(register ARMul_State *state, register ARMword instr)
{ /* SBC immed */
             dest = LHS - DPImmRHS - !CFLAG ;
             WRITEDEST(dest) ;
             return ;
}

void op0x2d(register ARMul_State *state, register ARMword instr)
{ /* SBCS immed */
             lhs = LHS ;
             rhs = DPImmRHS ;
             dest = lhs - rhs - !CFLAG ;
             if ((lhs >= rhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,lhs,rhs,dest) ;
                ARMul_SubOverflow(state,lhs,rhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x2e(register ARMul_State *state, register ARMword instr)
{ /* RSC immed */
             dest = DPImmRHS - LHS - !CFLAG ;
             WRITEDEST(dest) ;
             return ;
}

void op0x2f(register ARMul_State *state, register ARMword instr)
{ /* RSCS immed */
             lhs = LHS ;
             rhs = DPImmRHS ;
             dest = rhs - lhs - !CFLAG ;
             if ((rhs >= lhs) || ((rhs | lhs) >> 31)) {
                ARMul_SubCarry(state,rhs,lhs,dest) ;
                ARMul_SubOverflow(state,rhs,lhs,dest) ;
                }
             else {
                CLEARC ;
                CLEARV ;
                }
             WRITESDEST(dest) ;
             return ;
}

void op0x30(register ARMul_State *state, register ARMword instr)
{ /* TST immed */
             UNDEF_Test ;
             return ;
}

void op0x31(register ARMul_State *state, register ARMword instr)
{ /* TSTP immed */
             if (DESTReg == 15) { /* TSTP immed */
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                temp = LHS & DPImmRHS ;
                SETR15PSR(temp) ;
#endif
                }
             else {
                DPSImmRHS ; /* TST immed */
                dest = LHS & rhs ;
                ARMul_NegZero(state,dest) ;
                }
             return ;
}

void op0x32(register ARMul_State *state, register ARMword instr)
{ /* TEQ immed and MSR immed to CPSR */
             if (DESTReg==15 && BITS(17,18)==0) { /* MSR immed to CPSR */
                ARMul_FixCPSR(state,instr,DPImmRHS) ;
                }
             else {
                UNDEF_Test ;
                }
             return ;
}

void op0x33(register ARMul_State *state, register ARMword instr)
{ /* TEQP immed */
             if (DESTReg == 15) { /* TEQP immed */
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                temp = LHS ^ DPImmRHS ;
                SETR15PSR(temp) ;
#endif
                }
             else {
                DPSImmRHS ; /* TEQ immed */
                dest = LHS ^ rhs ;
                ARMul_NegZero(state,dest) ;
                }
             return ;
}

void op0x34(register ARMul_State *state, register ARMword instr)
{ /* CMP immed */
             UNDEF_Test ;
             return ;
}

void op0x35(register ARMul_State *state, register ARMword instr)
{ /* CMPP immed */
             if (DESTReg == 15) { /* CMPP immed */
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                temp = LHS - DPImmRHS ;
                SETR15PSR(temp) ;
#endif
                return ;
                }
             else {
                lhs = LHS ; /* CMP immed */
                rhs = DPImmRHS ;
                dest = lhs - rhs ;
                ARMul_NegZero(state,dest) ;
                if ((lhs >= rhs) || ((rhs | lhs) >> 31)) {
                   ARMul_SubCarry(state,lhs,rhs,dest) ;
                   ARMul_SubOverflow(state,lhs,rhs,dest) ;
                   }
                else {
                   CLEARC ;
                   CLEARV ;
                   }
                }
             return ;
}

void op0x36(register ARMul_State *state, register ARMword instr)
{ /* CMN immed and MSR immed to SPSR */
             if (DESTReg==15 && BITS(17,18)==0) /* MSR */
                ARMul_FixSPSR(state, instr, DPImmRHS) ;
             else {
                UNDEF_Test ;
                }
             return ;
}

void op0x37(register ARMul_State *state, register ARMword instr)
{ /* CMNP immed */
             if (DESTReg == 15) { /* CMNP immed */
#ifdef MODE32
                state->Cpsr = GETSPSR(state->Bank) ;
                ARMul_CPSRAltered(state) ;
#else
                temp = LHS + DPImmRHS ;
                SETR15PSR(temp) ;
#endif
                return ;
                }
             else {
                lhs = LHS ; /* CMN immed */
                rhs = DPImmRHS ;
                dest = lhs + rhs ;
                ASSIGNZ(dest==0) ;
                if ((lhs | rhs) >> 30) { /* possible C,V,N to set */
                   ASSIGNN(NEG(dest)) ;
                   ARMul_AddCarry(state,lhs,rhs,dest) ;
                   ARMul_AddOverflow(state,lhs,rhs,dest) ;
                   }
                else {
                   CLEARN ;
                   CLEARC ;
                   CLEARV ;
                   }
                }
             return ;
}

void op0x38(register ARMul_State *state, register ARMword instr)
{ /* ORR immed */
             dest = LHS | DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x39(register ARMul_State *state, register ARMword instr)
{ /* ORRS immed */
             DPSImmRHS ;
             dest = LHS | rhs ;
             WRITESDEST(dest) ;
             return ;
}

void op0x3a(register ARMul_State *state, register ARMword instr)
{ /* MOV immed */
             dest = DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x3b(register ARMul_State *state, register ARMword instr)
{ /* MOVS immed */
             DPSImmRHS ;
             WRITESDEST(rhs) ;
             return ;
}

void op0x3c(register ARMul_State *state, register ARMword instr)
{ /* BIC immed */
             dest = LHS & ~DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x3d(register ARMul_State *state, register ARMword instr)
{ /* BICS immed */
             DPSImmRHS ;
             dest = LHS & ~rhs ;
             WRITESDEST(dest) ;
             return ;
}

void op0x3e(register ARMul_State *state, register ARMword instr)
{ /* MVN immed */
             dest = ~DPImmRHS ;
             WRITEDEST(dest) ;
             return ;
}

void op0x3f(register ARMul_State *state, register ARMword instr)
{ /* MVNS immed */
             DPSImmRHS ;
             WRITESDEST(~rhs) ;
             return ;
}

/***************************************************************************\
*              Single Data Transfer Immediate RHS Instructions              *
\***************************************************************************/

void op0x40(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Post Dec, Immed */
             lhs = LHS ;
             if (StoreWord(state,instr,lhs))
                LSBase = lhs - LSImmRHS ;
             return ;
}

void op0x41(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Post Dec, Immed */
             lhs = LHS ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs - LSImmRHS ;
             return ;
}

void op0x42(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Post Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             temp = lhs - LSImmRHS ;
             state->NtransSig = LOW ;
             if (StoreWord(state,instr,lhs))
                LSBase = temp ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x43(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Post Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs - LSImmRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x44(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Post Dec, Immed */
             lhs = LHS ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs - LSImmRHS ;
             return ;
}

void op0x45(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Post Dec, Immed */
             lhs = LHS ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs - LSImmRHS ;
             return ;
}

void op0x46(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Post Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs - LSImmRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x47(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Post Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs - LSImmRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x48(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Post Inc, Immed */
             lhs = LHS ;
             if (StoreWord(state,instr,lhs))
                LSBase = lhs + LSImmRHS ;
             return ;
}

void op0x49(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Post Inc, Immed */
             lhs = LHS ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs + LSImmRHS ;
             return ;
}

void op0x4a(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Post Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (StoreWord(state,instr,lhs))
                LSBase = lhs + LSImmRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x4b(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Post Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs + LSImmRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x4c(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Post Inc, Immed */
             lhs = LHS ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs + LSImmRHS ;
             return ;
}

void op0x4d(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Post Inc, Immed */
             lhs = LHS ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs + LSImmRHS ;
             return ;
}

void op0x4e(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Post Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs + LSImmRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x4f(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Post Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs + LSImmRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x50(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Pre Dec, Immed */
             (void)StoreWord(state,instr,LHS - LSImmRHS) ;
             return ;
}

void op0x51(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Pre Dec, Immed */
             (void)LoadWord(state,instr,LHS - LSImmRHS) ;
             return ;
}

void op0x52(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Pre Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS - LSImmRHS ;
             if (StoreWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x53(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Pre Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS - LSImmRHS ;
             if (LoadWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x54(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Pre Dec, Immed */
             (void)StoreByte(state,instr,LHS - LSImmRHS) ;
             return ;
}

void op0x55(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Pre Dec, Immed */
             (void)LoadByte(state,instr,LHS - LSImmRHS,LUNSIGNED) ;
             return ;
}

void op0x56(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Pre Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS - LSImmRHS ;
             if (StoreByte(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x57(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Pre Dec, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS - LSImmRHS ;
             if (LoadByte(state,instr,temp,LUNSIGNED))
                LSBase = temp ;
             return ;
}

void op0x58(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Pre Inc, Immed */
             (void)StoreWord(state,instr,LHS + LSImmRHS) ;
             return ;
}

void op0x59(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Pre Inc, Immed */
             (void)LoadWord(state,instr,LHS + LSImmRHS) ;
             return ;
}

void op0x5a(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Pre Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS + LSImmRHS ;
             if (StoreWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x5b(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Pre Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS + LSImmRHS ;
             if (LoadWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x5c(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Pre Inc, Immed */
             (void)StoreByte(state,instr,LHS + LSImmRHS) ;
             return ;
}

void op0x5d(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Pre Inc, Immed */
             (void)LoadByte(state,instr,LHS + LSImmRHS,LUNSIGNED) ;
             return ;
}

void op0x5e(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Pre Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS + LSImmRHS ;
             if (StoreByte(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x5f(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Pre Inc, Immed */
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             temp = LHS + LSImmRHS ;
             if (LoadByte(state,instr,temp,LUNSIGNED))
                LSBase = temp ;
             return ;
}

/***************************************************************************\
*              Single Data Transfer Register RHS Instructions               *
\***************************************************************************/

void op0x60(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (StoreWord(state,instr,lhs))
                LSBase = lhs - LSRegRHS ;
             return ;
}

void op0x61(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs - LSRegRHS ;
             return ;
}

void op0x62(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (StoreWord(state,instr,lhs))
                LSBase = lhs - LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x63(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs - LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x64(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs - LSRegRHS ;
             return ;
}

void op0x65(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs - LSRegRHS ;
             return ;
}

void op0x66(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs - LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x67(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Post Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs - LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x68(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (StoreWord(state,instr,lhs))
                LSBase = lhs + LSRegRHS ;
             return ;
}

void op0x69(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs + LSRegRHS ;
             return ;
}

void op0x6a(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (StoreWord(state,instr,lhs))
                LSBase = lhs + LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x6b(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadWord(state,instr,lhs))
                LSBase = lhs + LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x6c(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs + LSRegRHS ;
             return ;
}

void op0x6d(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs + LSRegRHS ;
             return ;
}

void op0x6e(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (StoreByte(state,instr,lhs))
                LSBase = lhs + LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x6f(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Post Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             lhs = LHS ;
             state->NtransSig = LOW ;
             if (LoadByte(state,instr,lhs,LUNSIGNED))
                LSBase = lhs + LSRegRHS ;
             state->NtransSig = (state->Mode & 3)?HIGH:LOW ;
             return ;
}

void op0x70(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)StoreWord(state,instr,LHS - LSRegRHS) ;
             return ;
}

void op0x71(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)LoadWord(state,instr,LHS - LSRegRHS) ;
             return ;
}

void op0x72(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS - LSRegRHS ;
             if (StoreWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x73(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS - LSRegRHS ;
             if (LoadWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x74(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)StoreByte(state,instr,LHS - LSRegRHS) ;
             return ;
}

void op0x75(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)LoadByte(state,instr,LHS - LSRegRHS,LUNSIGNED) ;
             return ;
}

void op0x76(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS - LSRegRHS ;
             if (StoreByte(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x77(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Pre Dec, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS - LSRegRHS ;
             if (LoadByte(state,instr,temp,LUNSIGNED))
                LSBase = temp ;
             return ;
}

void op0x78(register ARMul_State *state, register ARMword instr)
{ /* Store Word, No WriteBack, Pre Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)StoreWord(state,instr,LHS + LSRegRHS) ;
             return ;
}

void op0x79(register ARMul_State *state, register ARMword instr)
{ /* Load Word, No WriteBack, Pre Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)LoadWord(state,instr,LHS + LSRegRHS) ;
             return ;
}

void op0x7a(register ARMul_State *state, register ARMword instr)
{ /* Store Word, WriteBack, Pre Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS + LSRegRHS ;
             if (StoreWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x7b(register ARMul_State *state, register ARMword instr)
{ /* Load Word, WriteBack, Pre Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS + LSRegRHS ;
             if (LoadWord(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x7c(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, No WriteBack, Pre Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)StoreByte(state,instr,LHS + LSRegRHS) ;
             return ;
}

void op0x7d(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, No WriteBack, Pre Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             (void)LoadByte(state,instr,LHS + LSRegRHS,LUNSIGNED) ;
             return ;
}

void op0x7e(register ARMul_State *state, register ARMword instr)
{ /* Store Byte, WriteBack, Pre Inc, Reg */
             if (BIT(4)) {
                ARMul_UndefInstr(state,instr) ;
                return ;
                }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS + LSRegRHS ;
             if (StoreByte(state,instr,temp))
                LSBase = temp ;
             return ;
}

void op0x7f(register ARMul_State *state, register ARMword instr)
{ /* Load Byte, WriteBack, Pre Inc, Reg */
             if (BIT(4))
	       {
		 /* Check for the special returnpoint opcode.
		    This value should correspond to the value defined
		    as ARM_BE_returnPOINT in gdb/arm-tdep.c.  */
		 if (BITS (0,19) == 0xfdefe)
		   {
//		     if (! ARMul_OSHandleSWI (state, SWI_Breakpoint))
		       ARMul_Abort (state, ARMul_SWIV);
		   }
		 else
		   ARMul_UndefInstr(state,instr) ;
		 return ;
	       }
             UNDEF_LSRBaseEQOffWb ;
             UNDEF_LSRBaseEQDestWb ;
             UNDEF_LSRPCBaseWb ;
             UNDEF_LSRPCOffWb ;
             temp = LHS + LSRegRHS ;
             if (LoadByte(state,instr,temp,LUNSIGNED))
                LSBase = temp ;
             return ;
}

/***************************************************************************\
*                   Multiple Data Transfer Instructions                     *
\***************************************************************************/

void op0x80(register ARMul_State *state, register ARMword instr)
{ /* Store, No WriteBack, Post Dec */
             STOREMULT(instr,LSBase - LSMNumRegs + 4L,0L) ;
             return ;
}

void op0x81(register ARMul_State *state, register ARMword instr)
{ /* Load, No WriteBack, Post Dec */
             LOADMULT(instr,LSBase - LSMNumRegs + 4L,0L) ;
             return ;
}

void op0x82(register ARMul_State *state, register ARMword instr)
{ /* Store, WriteBack, Post Dec */
             temp = LSBase - LSMNumRegs ;
             STOREMULT(instr,temp + 4L,temp) ;
             return ;
}

void op0x83(register ARMul_State *state, register ARMword instr)
{ /* Load, WriteBack, Post Dec */
             temp = LSBase - LSMNumRegs ;
             LOADMULT(instr,temp + 4L,temp) ;
             return ;
}

void op0x84(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, No WriteBack, Post Dec */
             STORESMULT(instr,LSBase - LSMNumRegs + 4L,0L) ;
             return ;
}

void op0x85(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, No WriteBack, Post Dec */
             LOADSMULT(instr,LSBase - LSMNumRegs + 4L,0L) ;
             return ;
}

void op0x86(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, WriteBack, Post Dec */
             temp = LSBase - LSMNumRegs ;
             STORESMULT(instr,temp + 4L,temp) ;
             return ;
}

void op0x87(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, WriteBack, Post Dec */
             temp = LSBase - LSMNumRegs ;
             LOADSMULT(instr,temp + 4L,temp) ;
             return ;
}

void op0x88(register ARMul_State *state, register ARMword instr)
{ /* Store, No WriteBack, Post Inc */
             STOREMULT(instr,LSBase,0L) ;
             return ;
}

void op0x89(register ARMul_State *state, register ARMword instr)
{ /* Load, No WriteBack, Post Inc */
             LOADMULT(instr,LSBase,0L) ;
             return ;
}

void op0x8a(register ARMul_State *state, register ARMword instr)
{ /* Store, WriteBack, Post Inc */
             temp = LSBase ;
             STOREMULT(instr,temp,temp + LSMNumRegs) ;
             return ;
}

void op0x8b(register ARMul_State *state, register ARMword instr)
{ /* Load, WriteBack, Post Inc */
             temp = LSBase ;
             LOADMULT(instr,temp,temp + LSMNumRegs) ;
             return ;
}

void op0x8c(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, No WriteBack, Post Inc */
             STORESMULT(instr,LSBase,0L) ;
             return ;
}

void op0x8d(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, No WriteBack, Post Inc */
             LOADSMULT(instr,LSBase,0L) ;
             return ;
}

void op0x8e(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, WriteBack, Post Inc */
             temp = LSBase ;
             STORESMULT(instr,temp,temp + LSMNumRegs) ;
             return ;
}

void op0x8f(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, WriteBack, Post Inc */
             temp = LSBase ;
             LOADSMULT(instr,temp,temp + LSMNumRegs) ;
             return ;
}

void op0x90(register ARMul_State *state, register ARMword instr)
{ /* Store, No WriteBack, Pre Dec */
             STOREMULT(instr,LSBase - LSMNumRegs,0L) ;
             return ;
}

void op0x91(register ARMul_State *state, register ARMword instr)
{ /* Load, No WriteBack, Pre Dec */
             LOADMULT(instr,LSBase - LSMNumRegs,0L) ;
             return ;
}

void op0x92(register ARMul_State *state, register ARMword instr)
{ /* Store, WriteBack, Pre Dec */
             temp = LSBase - LSMNumRegs ;
             STOREMULT(instr,temp,temp) ;
             return ;
}

void op0x93(register ARMul_State *state, register ARMword instr)
{ /* Load, WriteBack, Pre Dec */
             temp = LSBase - LSMNumRegs ;
             LOADMULT(instr,temp,temp) ;
             return ;
}

void op0x94(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, No WriteBack, Pre Dec */
             STORESMULT(instr,LSBase - LSMNumRegs,0L) ;
             return ;
}

void op0x95(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, No WriteBack, Pre Dec */
             LOADSMULT(instr,LSBase - LSMNumRegs,0L) ;
             return ;
}

void op0x96(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, WriteBack, Pre Dec */
             temp = LSBase - LSMNumRegs ;
             STORESMULT(instr,temp,temp) ;
             return ;
}

void op0x97(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, WriteBack, Pre Dec */
             temp = LSBase - LSMNumRegs ;
             LOADSMULT(instr,temp,temp) ;
             return ;
}

void op0x98(register ARMul_State *state, register ARMword instr)
{ /* Store, No WriteBack, Pre Inc */
             STOREMULT(instr,LSBase + 4L,0L) ;
             return ;
}

void op0x99(register ARMul_State *state, register ARMword instr)
{ /* Load, No WriteBack, Pre Inc */
             LOADMULT(instr,LSBase + 4L,0L) ;
             return ;
}

void op0x9a(register ARMul_State *state, register ARMword instr)
{ /* Store, WriteBack, Pre Inc */
             temp = LSBase ;
             STOREMULT(instr,temp + 4L,temp + LSMNumRegs) ;
             return ;
}

void op0x9b(register ARMul_State *state, register ARMword instr)
{ /* Load, WriteBack, Pre Inc */
             temp = LSBase ;
             LOADMULT(instr,temp + 4L,temp + LSMNumRegs) ;
             return ;
}

void op0x9c(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, No WriteBack, Pre Inc */
             STORESMULT(instr,LSBase + 4L,0L) ;
             return ;
}

void op0x9d(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, No WriteBack, Pre Inc */
             LOADSMULT(instr,LSBase + 4L,0L) ;
             return ;
}

void op0x9e(register ARMul_State *state, register ARMword instr)
{ /* Store, Flags, WriteBack, Pre Inc */
             temp = LSBase ;
             STORESMULT(instr,temp + 4L,temp + LSMNumRegs) ;
             return ;
}

void op0x9f(register ARMul_State *state, register ARMword instr)
{ /* Load, Flags, WriteBack, Pre Inc */
             temp = LSBase ;
             LOADSMULT(instr,temp + 4L,temp + LSMNumRegs) ;
             return ;
}

/***************************************************************************\
*                            Branch forward                                 *
\***************************************************************************/

#define op0xa1	op0xa0
#define op0xa2	op0xa0
#define op0xa3	op0xa0
#define op0xa4	op0xa0
#define op0xa5	op0xa0
#define op0xa6	op0xa0
#define op0xa7	op0xa0
void op0xa0(register ARMul_State *state, register ARMword instr)
{
             state->Reg[15] = pc + 8 + POSBRANCH ;
             FLUSHPIPE ;
             return ;
}

/***************************************************************************\
*                           Branch backward                                 *
\***************************************************************************/
#define op0xa9	op0xa8
#define op0xaa	op0xa8
#define op0xab	op0xa8
#define op0xac	op0xa8
#define op0xad	op0xa8
#define op0xae	op0xa8
#define op0xaf	op0xa8
void op0xa8(register ARMul_State *state, register ARMword instr)
{
             state->Reg[15] = pc + 8 + NEGBRANCH ;
             FLUSHPIPE ;
             return ;
}

/***************************************************************************\
*                       Branch and Link forward                             *
\***************************************************************************/

#define op0xb1	op0xb0
#define op0xb2	op0xb0
#define op0xb3	op0xb0
#define op0xb4	op0xb0
#define op0xb5	op0xb0
#define op0xb6	op0xb0
#define op0xb7	op0xb0
void op0xb0(register ARMul_State *state, register ARMword instr)
{
#ifdef MODE32
             state->Reg[14] = pc + 4 ; /* put PC into Link */
#else
             state->Reg[14] = (pc + 4) | ECC | ER15INT | EMODE ; /* put PC into Link */
#endif
             state->Reg[15] = pc + 8 + POSBRANCH ;
             FLUSHPIPE ;
             return ;
}

/***************************************************************************\
*                       Branch and Link backward                            *
\***************************************************************************/

#define op0xb9	op0xb8
#define op0xba	op0xb8
#define op0xbb	op0xb8
#define op0xbc	op0xb8
#define op0xbd	op0xb8
#define op0xbe	op0xb8
#define op0xbf	op0xb8
void op0xb8(register ARMul_State *state, register ARMword instr)
{
#ifdef MODE32
             state->Reg[14] = pc + 4 ; /* put PC into Link */
#else
             state->Reg[14] = (pc + 4) | ECC | ER15INT | EMODE ; /* put PC into Link */
#endif
             state->Reg[15] = pc + 8 + NEGBRANCH ;
             FLUSHPIPE ;
             return ;
}

/***************************************************************************\
*                        Co-Processor Data Transfers                        *
\***************************************************************************/

#define op0xc4	op0xc0
void op0xc0(register ARMul_State *state, register ARMword instr)
{ /* Store , No WriteBack , Post Dec */
             ARMul_STC(state,instr,LHS) ;
             return ;
}

#define op0xc5	op0xc1
void op0xc1(register ARMul_State *state, register ARMword instr)
{ /* Load , No WriteBack , Post Dec */
             ARMul_LDC(state,instr,LHS) ;
             return ;
}

#define op0xc6	op0xc2
void op0xc2(register ARMul_State *state, register ARMword instr)
{ /* Store , WriteBack , Post Dec */
             lhs = LHS ;
             state->Base = lhs - LSCOff ;
             ARMul_STC(state,instr,lhs) ;
             return ;
}

#define op0xc7	op0xc3
void op0xc3(register ARMul_State *state, register ARMword instr)
{ /* Load , WriteBack , Post Dec */
             lhs = LHS ;
             state->Base = lhs - LSCOff ;
             ARMul_LDC(state,instr,lhs) ;
             return ;
}

#define op0xcc	op0xc8
void op0xc8(register ARMul_State *state, register ARMword instr)
{ /* Store , No WriteBack , Post Inc */
             ARMul_STC(state,instr,LHS) ;
             return ;
}

#define op0xcd	op0xc9
void op0xc9(register ARMul_State *state, register ARMword instr)
{ /* Load , No WriteBack , Post Inc */
             ARMul_LDC(state,instr,LHS) ;
             return ;
}

#define op0xce	op0xca
void op0xca(register ARMul_State *state, register ARMword instr)
{ /* Store , WriteBack , Post Inc */
             lhs = LHS ;
             state->Base = lhs + LSCOff ;
             ARMul_STC(state,instr,LHS) ;
             return ;
}

#define op0xcf	op0xcb
void op0xcb(register ARMul_State *state, register ARMword instr)
{/* Load , WriteBack , Post Inc */
             lhs = LHS ;
             state->Base = lhs + LSCOff ;
             ARMul_LDC(state,instr,LHS) ;
             return ;
}

#define op0xd4	op0xd0
void op0xd0(register ARMul_State *state, register ARMword instr)
{ /* Store , No WriteBack , Pre Dec */
             ARMul_STC(state,instr,LHS - LSCOff) ;
             return ;
}

#define op0xd5	op0xd1
void op0xd1(register ARMul_State *state, register ARMword instr)
{ /* Load , No WriteBack , Pre Dec */
             ARMul_LDC(state,instr,LHS - LSCOff) ;
             return ;
}

#define op0xd6	op0xd2
void op0xd2(register ARMul_State *state, register ARMword instr)
{ /* Store , WriteBack , Pre Dec */
             lhs = LHS - LSCOff ;
             state->Base = lhs ;
             ARMul_STC(state,instr,lhs) ;
             return ;
}

#define op0xd7	op0xd3
void op0xd3(register ARMul_State *state, register ARMword instr)
{ /* Load , WriteBack , Pre Dec */
             lhs = LHS - LSCOff ;
             state->Base = lhs ;
             ARMul_LDC(state,instr,lhs) ;
             return ;
}

#define op0xdc	op0xd8
void op0xd8(register ARMul_State *state, register ARMword instr)
{ /* Store , No WriteBack , Pre Inc */
             ARMul_STC(state,instr,LHS + LSCOff) ;
             return ;
}

#define op0xdd	op0xd9
void op0xd9(register ARMul_State *state, register ARMword instr)
{ /* Load , No WriteBack , Pre Inc */
             ARMul_LDC(state,instr,LHS + LSCOff) ;
             return ;
}

#define op0xde	op0xda
void op0xda(register ARMul_State *state, register ARMword instr)
{ /* Store , WriteBack , Pre Inc */
             lhs = LHS + LSCOff ;
             state->Base = lhs ;
             ARMul_STC(state,instr,lhs) ;
             return ;
}

#define op0xdf	op0xdb
void op0xdb(register ARMul_State *state, register ARMword instr)
{ /* Load , WriteBack , Pre Inc */
             lhs = LHS + LSCOff ;
             state->Base = lhs ;
             ARMul_LDC(state,instr,lhs) ;
             return ;
}

/***************************************************************************\
*            Co-Processor Register Transfers (MCR) and Data Ops             *
\***************************************************************************/
#define op0xe2	op0xe0
#define op0xe4	op0xe0
#define op0xe6	op0xe0
#define op0xe8	op0xe0
#define op0xea	op0xe0
#define op0xec	op0xe0
#define op0xee	op0xe0
void op0xe0(register ARMul_State *state, register ARMword instr)
{
             if (BIT(4)) { /* MCR */
                if (DESTReg == 15) {
                   UNDEF_MCRPC ;
#ifdef MODE32
                   ARMul_MCR(state,instr,state->Reg[15] + isize) ;
#else
                   ARMul_MCR(state,instr,ECC | ER15INT | EMODE |
                                          ((state->Reg[15] + isize) & R15PCBITS) ) ;
#endif
                   }
                else
                   ARMul_MCR(state,instr,DEST) ;
                }
             else /* CDP Part 1 */
                ARMul_CDP(state,instr) ;
             return ;
}

/***************************************************************************\
*            Co-Processor Register Transfers (MRC) and Data Ops             *
\***************************************************************************/
#define op0xe3	op0xe1
#define op0xe5	op0xe1
#define op0xe7	op0xe1
#define op0xe9	op0xe1
#define op0xeb	op0xe1
#define op0xed	op0xe1
#define op0xef	op0xe1
void op0xe1(register ARMul_State *state, register ARMword instr)
{
             if (BIT(4)) { /* MRC */
                temp = ARMul_MRC(state,instr) ;
                if (DESTReg == 15) {
                   ASSIGNN((temp & NBIT) != 0) ;
                   ASSIGNZ((temp & ZBIT) != 0) ;
                   ASSIGNC((temp & CBIT) != 0) ;
                   ASSIGNV((temp & VBIT) != 0) ;
                   }
                else
                   DEST = temp ;
                }
             else /* CDP Part 2 */
                ARMul_CDP(state,instr) ;
             return ;
}

/***************************************************************************\
*                             SWI instruction                               *
\***************************************************************************/
#define op0xf1	op0xf0
#define op0xf2	op0xf0
#define op0xf3	op0xf0
#define op0xf4	op0xf0
#define op0xf5	op0xf0
#define op0xf6	op0xf0
#define op0xf7	op0xf0
#define op0xf8	op0xf0
#define op0xf9	op0xf0
#define op0xfa	op0xf0
#define op0xfb	op0xf0
#define op0xfc	op0xf0
#define op0xfd	op0xf0
#define op0xfe	op0xf0
#define op0xff	op0xf0
void op0xf0(register ARMul_State *state, register ARMword instr)
{
             if (instr == ARMul_ABORTWORD && state->AbortAddr == pc) { /* a prefetch abort */
                ARMul_Abort(state,ARMul_PrefetchAbortV) ;
                return ;
                }
    
#if 0
             if (!ARMul_OSHandleSWI(state,BITS(0,23))) {
                ARMul_Abort(state,ARMul_SWIV) ;
                }
#else
             ARMul_Abort(state,ARMul_SWIV) ;
#endif
             return ;
}

/***************************************************************************\
*                             EMULATION of ARM6                             *
\***************************************************************************/

/* The PC pipeline value depends on whether ARM or Thumb instructions
   are being executed: */
ARMword isize;


typedef void (op_func)(register ARMul_State *, register ARMword);

op_func *op[256] = {
	op0x00, op0x01, op0x02, op0x03, op0x04, op0x05, op0x06, op0x07,
	op0x08, op0x09, op0x0a, op0x0b, op0x0c, op0x0d, op0x0e, op0x0f,
	op0x10, op0x11, op0x12, op0x13, op0x14, op0x15, op0x16, op0x17,
	op0x18, op0x19, op0x1a, op0x1b, op0x1c, op0x1d, op0x1e, op0x1f,
	op0x20, op0x21, op0x22, op0x23, op0x24, op0x25, op0x26, op0x27,
	op0x28, op0x29, op0x2a, op0x2b, op0x2c, op0x2d, op0x2e, op0x2f,
	op0x30, op0x31, op0x32, op0x33, op0x34, op0x35, op0x36, op0x37,
	op0x38, op0x39, op0x3a, op0x3b, op0x3c, op0x3d, op0x3e, op0x3f,
	op0x40, op0x41, op0x42, op0x43, op0x44, op0x45, op0x46, op0x47,
	op0x48, op0x49, op0x4a, op0x4b, op0x4c, op0x4d, op0x4e, op0x4f,
	op0x50, op0x51, op0x52, op0x53, op0x54, op0x55, op0x56, op0x57,
	op0x58, op0x59, op0x5a, op0x5b, op0x5c, op0x5d, op0x5e, op0x5f,
	op0x60, op0x61, op0x62, op0x63, op0x64, op0x65, op0x66, op0x67,
	op0x68, op0x69, op0x6a, op0x6b, op0x6c, op0x6d, op0x6e, op0x6f,
	op0x70, op0x71, op0x72, op0x73, op0x74, op0x75, op0x76, op0x77,
	op0x78, op0x79, op0x7a, op0x7b, op0x7c, op0x7d, op0x7e, op0x7f,
	op0x80, op0x81, op0x82, op0x83, op0x84, op0x85, op0x86, op0x87,
	op0x88, op0x89, op0x8a, op0x8b, op0x8c, op0x8d, op0x8e, op0x8f,
	op0x90, op0x91, op0x92, op0x93, op0x94, op0x95, op0x96, op0x97,
	op0x98, op0x99, op0x9a, op0x9b, op0x9c, op0x9d, op0x9e, op0x9f,
	op0xa0, op0xa1, op0xa2, op0xa3, op0xa4, op0xa5, op0xa6, op0xa7,
	op0xa8, op0xa9, op0xaa, op0xab, op0xac, op0xad, op0xae, op0xaf,
	op0xb0, op0xb1, op0xb2, op0xb3, op0xb4, op0xb5, op0xb6, op0xb7,
	op0xb8, op0xb9, op0xba, op0xbb, op0xbc, op0xbd, op0xbe, op0xbf,
	op0xc0, op0xc1, op0xc2, op0xc3, op0xc4, op0xc5, op0xc6, op0xc7,
	op0xc8, op0xc9, op0xca, op0xcb, op0xcc, op0xcd, op0xce, op0xcf,
	op0xd0, op0xd1, op0xd2, op0xd3, op0xd4, op0xd5, op0xd6, op0xd7,
	op0xd8, op0xd9, op0xda, op0xdb, op0xdc, op0xdd, op0xde, op0xdf,
	op0xe0, op0xe1, op0xe2, op0xe3, op0xe4, op0xe5, op0xe6, op0xe7,
	op0xe8, op0xe9, op0xea, op0xeb, op0xec, op0xed, op0xee, op0xef,
	op0xf0, op0xf1, op0xf2, op0xf3, op0xf4, op0xf5, op0xf6, op0xf7,
	op0xf8, op0xf9, op0xfa, op0xfb, op0xfc, op0xfd, op0xfe, op0xff
};

#ifdef MODE32
ARMword ARMul_Emulate32(register ARMul_State *state)
{
#else
ARMword ARMul_Emulate26(register ARMul_State *state)
{
#endif
 register ARMword instr; /* the current instruction */

/***************************************************************************\
*                        Execute the next instruction                       *
\***************************************************************************/

 if (state->NextInstr < PRIMEPIPE) {
    decoded = state->decoded ;
    loaded = state->loaded ;
    pc = state->pc ;
    }

 do { /* just keep going */
#ifdef MODET
    if (TFLAG) {
     isize = 2;
    } else
#endif
     isize = 4;
    switch (state->NextInstr) {
       case SEQ :
          state->Reg[15] += isize ; /* Advance the pipeline, and an S cycle */
          pc += isize ;
          instr = decoded ;
          decoded = loaded ;
          loaded = ARMul_LoadInstrS(state,pc+(isize * 2),isize) ;
          break ;

       case NONSEQ :
          state->Reg[15] += isize ; /* Advance the pipeline, and an N cycle */
          pc += isize ;
          instr = decoded ;
          decoded = loaded ;
          loaded = ARMul_LoadInstrN(state,pc+(isize * 2),isize) ;
          NORMALCYCLE ;
          break ;

       case PCINCEDSEQ :
          pc += isize ; /* Program counter advanced, and an S cycle */
          instr = decoded ;
          decoded = loaded ;
          loaded = ARMul_LoadInstrS(state,pc+(isize * 2),isize) ;
          NORMALCYCLE ;
          break ;

       case PCINCEDNONSEQ :
          pc += isize ; /* Program counter advanced, and an N cycle */
          instr = decoded ;
          decoded = loaded ;
          loaded = ARMul_LoadInstrN(state,pc+(isize * 2),isize) ;
          NORMALCYCLE ;
          break ;

       case RESUME : /* The program counter has been changed */
          pc = state->Reg[15] ;
#ifndef MODE32
          pc = pc & R15PCBITS ;
#endif
          state->Reg[15] = pc + (isize * 2) ;
          state->Aborted = 0 ;
          instr = ARMul_ReLoadInstr(state,pc,isize) ;
          decoded = ARMul_ReLoadInstr(state,pc + isize,isize) ;
          loaded = ARMul_ReLoadInstr(state,pc + isize * 2,isize) ;
          NORMALCYCLE ;
          break ;

       default : /* The program counter has been changed */
          pc = state->Reg[15] ;
#ifndef MODE32
          pc = pc & R15PCBITS ;
#endif
          state->Reg[15] = pc + (isize * 2) ;
          state->Aborted = 0 ;
          instr = ARMul_LoadInstrN(state,pc,isize) ;
          decoded = ARMul_LoadInstrS(state,pc + (isize),isize) ;
          loaded = ARMul_LoadInstrS(state,pc + (isize * 2),isize) ;
          NORMALCYCLE ;
          break ;
       }
    if (state->EventSet)
       ARMul_EnvokeEvent(state) ;
    
#if 0
    /* Enable this for a helpful bit of debugging when tracing is needed.  */
    fprintf (stderr, "pc: %x, instr: %x\n", pc & ~1, instr);
    if (instr == 0) abort ();
#endif

    if (state->Exception) { /* Any exceptions */
       if (state->NresetSig == LOW) {
           ARMul_Abort(state,ARMul_ResetV) ;
           break ;
           }
       else if (!state->NfiqSig && !FFLAG) {
           ARMul_Abort(state,ARMul_FIQV) ;
           break ;
           }
       else if (!state->NirqSig && !IFLAG) {
          ARMul_Abort(state,ARMul_IRQV) ;
          break ;
          }
       }

    if (state->CallDebug > 0) {
       instr = ARMul_Debug(state,pc,instr) ;
       if (state->Emulate < ONCE) {
          state->NextInstr = RESUME ;
          break ;
          }
       if (state->Debug) {
          fprintf(stderr,"At %08lx Instr %08lx Mode %02lx\n",pc,instr,state->Mode) ;
//          (void)fgetc(stdin) ;
          }
       }
    else
       if (state->Emulate < ONCE) {
          state->NextInstr = RESUME ;
          break ;
          }
          
    io_do_cycle(state);

    state->NumInstrs++ ;

#ifdef MODET
 /* Provide Thumb instruction decoding. If the processor is in Thumb
    mode, then we can simply decode the Thumb instruction, and map it
    to the corresponding ARM instruction (by directly loading the
    instr variable, and letting the normal ARM simulator
    execute). There are some caveats to ensure that the correct
    pipelined PC value is used when executing Thumb code, and also for
    dealing with the BL instruction. */
    if (TFLAG) { /* check if in Thumb mode */
      ARMword new;
      switch (ARMul_ThumbDecode(state,pc,instr,&new)) {
        case t_undefined:
          ARMul_UndefInstr(state,instr); /* This is a Thumb instruction */
          break;

        case t_branch: /* already processed */
          goto donext;

        case t_decoded: /* ARM instruction available */
          instr = new; /* so continue instruction decoding */
          break;
      }
    }
#endif

/***************************************************************************\
*                       Check the condition codes                           *
\***************************************************************************/
    if ((temp = TOPBITS(28)) == AL)
       goto mainswitch ; /* vile deed in the need for speed */

    switch ((int)TOPBITS(28)) { /* check the condition code */
       case AL : temp=TRUE ;
                 break ;
       case NV : temp=FALSE ;
                 break ;
       case EQ : temp=ZFLAG ;
                 break ;
       case NE : temp=!ZFLAG ;
                 break ;
       case VS : temp=VFLAG ;
                 break ;
       case VC : temp=!VFLAG ;
                 break ;
       case MI : temp=NFLAG ;
                 break ;
       case PL : temp=!NFLAG ;
                 break ;
       case CS : temp=CFLAG ;
                 break ;
       case CC : temp=!CFLAG ;
                 break ;
       case HI : temp=(CFLAG && !ZFLAG) ;
                 break ;
       case LS : temp=(!CFLAG || ZFLAG) ;
                 break ;
       case GE : temp=((!NFLAG && !VFLAG) || (NFLAG && VFLAG)) ;
                 break ;
       case LT : temp=((NFLAG && !VFLAG) || (!NFLAG && VFLAG)) ;
                 break ;
       case GT : temp=((!NFLAG && !VFLAG && !ZFLAG) || (NFLAG && VFLAG && !ZFLAG)) ;
                 break ;
       case LE : temp=((NFLAG && !VFLAG) || (!NFLAG && VFLAG)) || ZFLAG ;
                 break ;
       } /* cc check */

/***************************************************************************\
*               Actual execution of instructions begins here                *
\***************************************************************************/

    if (temp) { /* if the condition codes don't match, stop here */
mainswitch:

       op[(int)BITS(20,27)](state,instr);
       } /* if temp */

#ifdef MODET
donext:
#endif

#ifdef NEED_UI_LOOP_HOOK
    if (ui_loop_hook != NULL && ui_loop_hook_counter-- < 0)
      {
	ui_loop_hook_counter = UI_LOOP_POLL_INTERVAL;
	ui_loop_hook (0);
      }
#endif /* NEED_UI_LOOP_HOOK */

    if (state->Emulate == ONCE)
        state->Emulate = STOP;
    else if (state->Emulate != RUN)
        break;
    } while (!stop_simulator) ; /* do loop */

 state->decoded = decoded ;
 state->loaded = loaded ;
 state->pc = pc ;
 return(pc) ;
 } /* Emulate 26/32 in instruction based mode */


/***************************************************************************\
* This routine evaluates most Data Processing register RHS's with the S     *
* bit clear.  It is intended to be called from the macro DPRegRHS, which    *
* filters the common case of an unshifted register with in line code        *
\***************************************************************************/

static ARMword GetDPRegRHS(ARMul_State *state, ARMword instr)
{ARMword shamt , base ;

 base = RHSReg ;
 if (BIT(4)) { /* shift amount in a register */
    UNDEF_Shift ;
    INCPC ;
#ifndef MODE32
    if (base == 15)
       base = ECC | ER15INT | R15PC | EMODE ;
    else
#endif
       base = state->Reg[base] ;
    ARMul_Icycles(state,1,0L) ;
    shamt = state->Reg[BITS(8,11)] & 0xff ;
    switch ((int)BITS(5,6)) {
       case LSL : if (shamt == 0)
                     return(base) ;
                  else if (shamt >= 32)
                     return(0) ;
                  else
                     return(base << shamt) ;
       case LSR : if (shamt == 0)
                     return(base) ;
                  else if (shamt >= 32)
                     return(0) ;
                  else
                     return(base >> shamt) ;
       case ASR : if (shamt == 0)
                     return(base) ;
                  else if (shamt >= 32)
                     return((ARMword)((long int)base >> 31L)) ;
                  else
                     return((ARMword)((long int)base >> (int)shamt)) ;
       case ROR : shamt &= 0x1f ;
                  if (shamt == 0)
                     return(base) ;
                  else
                     return((base << (32 - shamt)) | (base >> shamt)) ;
       }
    }
 else { /* shift amount is a constant */
#ifndef MODE32
    if (base == 15)
       base = ECC | ER15INT | R15PC | EMODE ;
    else
#endif
       base = state->Reg[base] ;
    shamt = BITS(7,11) ;
    switch ((int)BITS(5,6)) {
       case LSL : return(base<<shamt) ;
       case LSR : if (shamt == 0)
                     return(0) ;
                  else
                     return(base >> shamt) ;
       case ASR : if (shamt == 0)
                     return((ARMword)((long int)base >> 31L)) ;
                  else
                     return((ARMword)((long int)base >> (int)shamt)) ;
       case ROR : if (shamt==0) /* its an RRX */
                     return((base >> 1) | (CFLAG << 31)) ;
                  else
                     return((base << (32 - shamt)) | (base >> shamt)) ;
       }
    }
 return(0) ; /* just to shut up lint */
 }
/***************************************************************************\
* This routine evaluates most Logical Data Processing register RHS's        *
* with the S bit set.  It is intended to be called from the macro           *
* DPSRegRHS, which filters the common case of an unshifted register         *
* with in line code                                                         *
\***************************************************************************/

static ARMword GetDPSRegRHS(ARMul_State *state, ARMword instr)
{ARMword shamt , base ;

 base = RHSReg ;
 if (BIT(4)) { /* shift amount in a register */
    UNDEF_Shift ;
    INCPC ;
#ifndef MODE32
    if (base == 15)
       base = ECC | ER15INT | R15PC | EMODE ;
    else
#endif
       base = state->Reg[base] ;
    ARMul_Icycles(state,1,0L) ;
    shamt = state->Reg[BITS(8,11)] & 0xff ;
    switch ((int)BITS(5,6)) {
       case LSL : if (shamt == 0)
                     return(base) ;
                  else if (shamt == 32) {
                     ASSIGNC(base & 1) ;
                     return(0) ;
                     }
                  else if (shamt > 32) {
                     CLEARC ;
                     return(0) ;
                     }
                  else {
                     ASSIGNC((base >> (32-shamt)) & 1) ;
                     return(base << shamt) ;
                     }
       case LSR : if (shamt == 0)
                     return(base) ;
                  else if (shamt == 32) {
                     ASSIGNC(base >> 31) ;
                     return(0) ;
                     }
                  else if (shamt > 32) {
                     CLEARC ;
                     return(0) ;
                     }
                  else {
                     ASSIGNC((base >> (shamt - 1)) & 1) ;
                     return(base >> shamt) ;
                     }
       case ASR : if (shamt == 0)
                     return(base) ;
                  else if (shamt >= 32) {
                     ASSIGNC(base >> 31L) ;
                     return((ARMword)((long int)base >> 31L)) ;
                     }
                  else {
                     ASSIGNC((ARMword)((long int)base >> (int)(shamt-1)) & 1) ;
                     return((ARMword)((long int)base >> (int)shamt)) ;
                     }
       case ROR : if (shamt == 0)
                     return(base) ;
                  shamt &= 0x1f ;
                  if (shamt == 0) {
                     ASSIGNC(base >> 31) ;
                     return(base) ;
                     }
                  else {
                     ASSIGNC((base >> (shamt-1)) & 1) ;
                     return((base << (32-shamt)) | (base >> shamt)) ;
                     }
       }
    }
 else { /* shift amount is a constant */
#ifndef MODE32
    if (base == 15)
       base = ECC | ER15INT | R15PC | EMODE ;
    else
#endif
       base = state->Reg[base] ;
    shamt = BITS(7,11) ;
    switch ((int)BITS(5,6)) {
       case LSL : ASSIGNC((base >> (32-shamt)) & 1) ;
                  return(base << shamt) ;
       case LSR : if (shamt == 0) {
                     ASSIGNC(base >> 31) ;
                     return(0) ;
                     }
                  else {
                     ASSIGNC((base >> (shamt - 1)) & 1) ;
                     return(base >> shamt) ;
                     }
       case ASR : if (shamt == 0) {
                     ASSIGNC(base >> 31L) ;
                     return((ARMword)((long int)base >> 31L)) ;
                     }
                  else {
                     ASSIGNC((ARMword)((long int)base >> (int)(shamt-1)) & 1) ;
                     return((ARMword)((long int)base >> (int)shamt)) ;
                     }
       case ROR : if (shamt == 0) { /* its an RRX */
                     shamt = CFLAG ;
                     ASSIGNC(base & 1) ;
                     return((base >> 1) | (shamt << 31)) ;
                     }
                  else {
                     ASSIGNC((base >> (shamt - 1)) & 1) ;
                     return((base << (32-shamt)) | (base >> shamt)) ;
                     }
       }
    }
 return(0) ; /* just to shut up lint */
 }

/***************************************************************************\
* This routine handles writes to register 15 when the S bit is not set.     *
\***************************************************************************/

static void WriteR15(ARMul_State *state, ARMword src)
{
  /* The ARM documentation implies (but doe snot state) that the bottom bit of the PC is never set */
#ifdef MODE32
 state->Reg[15] = src & PCBITS & ~ 0x1 ;
#else
 state->Reg[15] = (src & R15PCBITS & ~ 0x1) | ECC | ER15INT | EMODE ;
 ARMul_R15Altered(state) ;
#endif
 FLUSHPIPE ;
 }

/***************************************************************************\
* This routine handles writes to register 15 when the S bit is set.         *
\***************************************************************************/

static void WriteSR15(ARMul_State *state, ARMword src)
{
#ifdef MODE32
 state->Reg[15] = src & PCBITS ;
 if (state->Bank > 0) {
    state->Cpsr = state->Spsr[state->Bank] ;
    ARMul_CPSRAltered(state) ;
    }
#else
 if (state->Bank == USERBANK)
    state->Reg[15] = (src & (CCBITS | R15PCBITS)) | ER15INT | EMODE ;
 else
    state->Reg[15] = src ;
 ARMul_R15Altered(state) ;
#endif
 FLUSHPIPE ;
 }

/***************************************************************************\
* This routine evaluates most Load and Store register RHS's.  It is         *
* intended to be called from the macro LSRegRHS, which filters the          *
* common case of an unshifted register with in line code                    *
\***************************************************************************/

static ARMword GetLSRegRHS(ARMul_State *state, ARMword instr)
{ARMword shamt, base ;

 base = RHSReg ;
#ifndef MODE32
 if (base == 15)
    base = ECC | ER15INT | R15PC | EMODE ; /* Now forbidden, but .... */
 else
#endif
    base = state->Reg[base] ;

 shamt = BITS(7,11) ;
 switch ((int)BITS(5,6)) {
    case LSL : return(base << shamt) ;
    case LSR : if (shamt == 0)
                  return(0) ;
               else
                  return(base >> shamt) ;
    case ASR : if (shamt == 0)
                  return((ARMword)((long int)base >> 31L)) ;
               else
                  return((ARMword)((long int)base >> (int)shamt)) ;
    case ROR : if (shamt==0) /* its an RRX */
                  return((base >> 1) | (CFLAG << 31)) ;
               else
                  return((base << (32-shamt)) | (base >> shamt)) ;
    }
 return(0) ; /* just to shut up lint */
 }

/***************************************************************************\
* This routine evaluates the ARM7T halfword and signed transfer RHS's.      *
\***************************************************************************/

static ARMword GetLS7RHS(ARMul_State *state, ARMword instr)
{
 if (BIT(22) == 0) { /* register */
#ifndef MODE32
    if (RHSReg == 15)
      return ECC | ER15INT | R15PC | EMODE ; /* Now forbidden, but ... */
#endif
    return state->Reg[RHSReg] ;
    }

 /* else immediate */
 return BITS(0,3) | (BITS(8,11) << 4) ;
 }

/***************************************************************************\
* This function does the work of loading a word for a LDR instruction.      *
\***************************************************************************/

static unsigned LoadWord(ARMul_State *state, ARMword instr, ARMword address)
{
 ARMword dest ;

 BUSUSEDINCPCS ;
#ifndef MODE32
 if (ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    }
#endif
 dest = ARMul_LoadWordN(state,address) ;
 if (state->Aborted) {
    TAKEABORT ;
    return(state->lateabtSig) ;
    }
 if (address & 3)
    dest = ARMul_Align(state,address,dest) ;
 WRITEDEST(dest) ;
 ARMul_Icycles(state,1,0L) ;

 return(DESTReg != LHSReg) ;
}

#ifdef MODET
/***************************************************************************\
* This function does the work of loading a halfword.                        *
\***************************************************************************/

static unsigned LoadHalfWord(ARMul_State *state, ARMword instr, ARMword address,int signextend)
{
 ARMword dest ;

 BUSUSEDINCPCS ;
#ifndef MODE32
 if (ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    }
#endif
 dest = ARMul_LoadHalfWord(state,address) ;
 if (state->Aborted) {
    TAKEABORT ;
    return(state->lateabtSig) ;
    }
 UNDEF_LSRBPC ;
 if (signextend)
   {
     if (dest & 1 << (16 - 1))
         dest = (dest & ((1 << 16) - 1)) - (1 << 16) ;
   }
 WRITEDEST(dest) ;
 ARMul_Icycles(state,1,0L) ;
 return(DESTReg != LHSReg) ;
}
#endif /* MODET */

/***************************************************************************\
* This function does the work of loading a byte for a LDRB instruction.     *
\***************************************************************************/

static unsigned LoadByte(ARMul_State *state, ARMword instr, ARMword address,int signextend)
{
 ARMword dest ;

 BUSUSEDINCPCS ;
#ifndef MODE32
 if (ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    }
#endif
 dest = ARMul_LoadByte(state,address) ;
 if (state->Aborted) {
    TAKEABORT ;
    return(state->lateabtSig) ;
    }
 UNDEF_LSRBPC ;
 if (signextend)
   {
     if (dest & 1 << (8 - 1))
         dest = (dest & ((1 << 8) - 1)) - (1 << 8) ;
   }
 WRITEDEST(dest) ;
 ARMul_Icycles(state,1,0L) ;
 return(DESTReg != LHSReg) ;
}

/***************************************************************************\
* This function does the work of storing a word from a STR instruction.     *
\***************************************************************************/

static unsigned StoreWord(ARMul_State *state, ARMword instr, ARMword address)
{BUSUSEDINCPCN ;
#ifndef MODE32
 if (DESTReg == 15)
    state->Reg[15] = ECC | ER15INT | R15PC | EMODE ;
#endif
#ifdef MODE32
 ARMul_StoreWordN(state,address,DEST) ;
#else
 if (VECTORACCESS(address) || ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    (void)ARMul_LoadWordN(state,address) ;
    }
 else
    ARMul_StoreWordN(state,address,DEST) ;
#endif
 if (state->Aborted) {
    TAKEABORT ;
    return(state->lateabtSig) ;
    }
 return(TRUE) ;
}

#ifdef MODET
/***************************************************************************\
* This function does the work of storing a byte for a STRH instruction.     *
\***************************************************************************/

static unsigned StoreHalfWord(ARMul_State *state, ARMword instr, ARMword address)
{BUSUSEDINCPCN ;

#ifndef MODE32
 if (DESTReg == 15)
    state->Reg[15] = ECC | ER15INT | R15PC | EMODE ;
#endif

#ifdef MODE32
 ARMul_StoreHalfWord(state,address,DEST);
#else
 if (VECTORACCESS(address) || ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    (void)ARMul_LoadHalfWord(state,address) ;
    }
 else
    ARMul_StoreHalfWord(state,address,DEST) ;
#endif

 if (state->Aborted) {
    TAKEABORT ;
    return(state->lateabtSig) ;
    }

 return(TRUE) ;
}
#endif /* MODET */

/***************************************************************************\
* This function does the work of storing a byte for a STRB instruction.     *
\***************************************************************************/

static unsigned StoreByte(ARMul_State *state, ARMword instr, ARMword address)
{BUSUSEDINCPCN ;
#ifndef MODE32
 if (DESTReg == 15)
    state->Reg[15] = ECC | ER15INT | R15PC | EMODE ;
#endif
#ifdef MODE32
 ARMul_StoreByte(state,address,DEST) ;
#else
 if (VECTORACCESS(address) || ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    (void)ARMul_LoadByte(state,address) ;
    }
 else
    ARMul_StoreByte(state,address,DEST) ;
#endif
 if (state->Aborted) {
    TAKEABORT ;
    return(state->lateabtSig) ;
    }
 UNDEF_LSRBPC ;
 return(TRUE) ;
}

/***************************************************************************\
* This function does the work of loading the registers listed in an LDM     *
* instruction, when the S bit is clear.  The code here is always increment  *
* after, it's up to the caller to get the input address correct and to      *
* handle base register modification.                                        *
\***************************************************************************/

static void LoadMult(ARMul_State *state, ARMword instr,
                     ARMword address, ARMword WBBase)
{ARMword dest, temp ;

 UNDEF_LSMNoRegs ;
 UNDEF_LSMPCBase ;
 UNDEF_LSMBaseInListWb ;
 BUSUSEDINCPCS ;
#ifndef MODE32
 if (ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    }
#endif
 if (BIT(21) && LHSReg != 15)
    LSBase = WBBase ;

    for (temp = 0 ; !BIT(temp) ; temp++) ; /* N cycle first */
    dest = ARMul_LoadWordN(state,address) ;
    if (!state->abortSig && !state->Aborted)
       state->Reg[temp++] = dest ;
    else
       if (!state->Aborted)
          state->Aborted = ARMul_DataAbortV ;

    for (; temp < 16 ; temp++) /* S cycles from here on */
       if (BIT(temp)) { /* load this register */
          address += 4 ;
          dest = ARMul_LoadWordS(state,address) ;
          if (!state->abortSig && !state->Aborted)
             state->Reg[temp] = dest ;
          else
             if (!state->Aborted)
                state->Aborted = ARMul_DataAbortV ;
          }

 if (BIT(15)) { /* PC is in the reg list */
#ifdef MODE32
    state->Reg[15] = PC ;
#endif
    FLUSHPIPE ;
    }

 ARMul_Icycles(state,1,0L) ; /* to write back the final register */

 if (state->Aborted) {
    if (BIT(21) && LHSReg != 15)
       LSBase = WBBase ;
    TAKEABORT ;
    }
 }

/***************************************************************************\
* This function does the work of loading the registers listed in an LDM     *
* instruction, when the S bit is set. The code here is always increment     *
* after, it's up to the caller to get the input address correct and to      *
* handle base register modification.                                        *
\***************************************************************************/

static void LoadSMult(ARMul_State *state, ARMword instr,
                      ARMword address, ARMword WBBase)
{ARMword dest, temp ;

 UNDEF_LSMNoRegs ;
 UNDEF_LSMPCBase ;
 UNDEF_LSMBaseInListWb ;
 BUSUSEDINCPCS ;
#ifndef MODE32
 if (ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    }
#endif

 if (!BIT(15) && state->Bank != USERBANK) {
    (void)ARMul_SwitchMode(state,state->Mode,USER26MODE) ; /* temporary reg bank switch */
    UNDEF_LSMUserBankWb ;
    }

 if (BIT(21) && LHSReg != 15)
    LSBase = WBBase ;

    for (temp = 0 ; !BIT(temp) ; temp++) ; /* N cycle first */
    dest = ARMul_LoadWordN(state,address) ;
    if (!state->abortSig)
       state->Reg[temp++] = dest ;
    else
       if (!state->Aborted)
          state->Aborted = ARMul_DataAbortV ;

    for (; temp < 16 ; temp++) /* S cycles from here on */
       if (BIT(temp)) { /* load this register */
          address += 4 ;
          dest = ARMul_LoadWordS(state,address) ;
          if (!state->abortSig || state->Aborted)
             state->Reg[temp] = dest ;
          else
             if (!state->Aborted)
                state->Aborted = ARMul_DataAbortV ;
          }

 if (BIT(15)) { /* PC is in the reg list */
#ifdef MODE32
    if (state->Mode != USER26MODE && state->Mode != USER32MODE) {
       state->Cpsr = GETSPSR(state->Bank) ;
       ARMul_CPSRAltered(state) ;
       }
    state->Reg[15] = PC ;
#else
    if (state->Mode == USER26MODE || state->Mode == USER32MODE) { /* protect bits in user mode */
       ASSIGNN((state->Reg[15] & NBIT) != 0) ;
       ASSIGNZ((state->Reg[15] & ZBIT) != 0) ;
       ASSIGNC((state->Reg[15] & CBIT) != 0) ;
       ASSIGNV((state->Reg[15] & VBIT) != 0) ;
       }
    else
       ARMul_R15Altered(state) ;
#endif
    FLUSHPIPE ;
    }

 if (!BIT(15) && state->Mode != USER26MODE && state->Mode != USER32MODE)
    (void)ARMul_SwitchMode(state,USER26MODE,state->Mode) ; /* restore the correct bank */

 ARMul_Icycles(state,1,0L) ; /* to write back the final register */

 if (state->Aborted) {
    if (BIT(21) && LHSReg != 15)
       LSBase = WBBase ;
    TAKEABORT ;
    }

}

/***************************************************************************\
* This function does the work of storing the registers listed in an STM     *
* instruction, when the S bit is clear.  The code here is always increment  *
* after, it's up to the caller to get the input address correct and to      *
* handle base register modification.                                        *
\***************************************************************************/

static void StoreMult(ARMul_State *state, ARMword instr,
                      ARMword address, ARMword WBBase)
{ARMword temp ;

 UNDEF_LSMNoRegs ;
 UNDEF_LSMPCBase ;
 UNDEF_LSMBaseInListWb ;
#ifdef MODET
 if (!TFLAG) {
   BUSUSEDINCPCN ; /* N-cycle, increment the PC and update the NextInstr state */
 }
#endif
#ifndef MODE32
 if (VECTORACCESS(address) || ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    }
 if (BIT(15))
    PATCHR15 ;
#endif

 for (temp = 0 ; !BIT(temp) ; temp++) ; /* N cycle first */
#ifdef MODE32
 ARMul_StoreWordN(state,address,state->Reg[temp++]) ;
#else
 if (state->Aborted) {
    (void)ARMul_LoadWordN(state,address) ;
    for ( ; temp < 16 ; temp++) /* Fake the Stores as Loads */
       if (BIT(temp)) { /* save this register */
          address += 4 ;
          (void)ARMul_LoadWordS(state,address) ;
          }
    if (BIT(21) && LHSReg != 15)
       LSBase = WBBase ;
    TAKEABORT ;
    return ;
    }
 else
    ARMul_StoreWordN(state,address,state->Reg[temp++]) ;
#endif
 if (state->abortSig && !state->Aborted)
    state->Aborted = ARMul_DataAbortV ;

 if (BIT(21) && LHSReg != 15)
    LSBase = WBBase ;

 for ( ; temp < 16 ; temp++) /* S cycles from here on */
    if (BIT(temp)) { /* save this register */
       address += 4 ;
       ARMul_StoreWordS(state,address,state->Reg[temp]) ;
       if (state->abortSig && !state->Aborted)
             state->Aborted = ARMul_DataAbortV ;
       }
    if (state->Aborted) {
       TAKEABORT ;
       }
 }

/***************************************************************************\
* This function does the work of storing the registers listed in an STM     *
* instruction when the S bit is set.  The code here is always increment     *
* after, it's up to the caller to get the input address correct and to      *
* handle base register modification.                                        *
\***************************************************************************/

static void StoreSMult(ARMul_State *state, ARMword instr,
                       ARMword address, ARMword WBBase)
{ARMword temp ;

 UNDEF_LSMNoRegs ;
 UNDEF_LSMPCBase ;
 UNDEF_LSMBaseInListWb ;
 BUSUSEDINCPCN ;
#ifndef MODE32
 if (VECTORACCESS(address) || ADDREXCEPT(address)) {
    INTERNALABORT(address) ;
    }
 if (BIT(15))
    PATCHR15 ;
#endif

 if (state->Bank != USERBANK) {
    (void)ARMul_SwitchMode(state,state->Mode,USER26MODE) ; /* Force User Bank */
    UNDEF_LSMUserBankWb ;
    }

 for (temp = 0 ; !BIT(temp) ; temp++) ; /* N cycle first */
#ifdef MODE32
 ARMul_StoreWordN(state,address,state->Reg[temp++]) ;
#else
 if (state->Aborted) {
    (void)ARMul_LoadWordN(state,address) ;
    for ( ; temp < 16 ; temp++) /* Fake the Stores as Loads */
       if (BIT(temp)) { /* save this register */
          address += 4 ;
          (void)ARMul_LoadWordS(state,address) ;
          }
    if (BIT(21) && LHSReg != 15)
       LSBase = WBBase ;
    TAKEABORT ;
    return ;
    }
 else
    ARMul_StoreWordN(state,address,state->Reg[temp++]) ;
#endif
 if (state->abortSig && !state->Aborted)
    state->Aborted = ARMul_DataAbortV ;

 if (BIT(21) && LHSReg != 15)
    LSBase = WBBase ;

 for (; temp < 16 ; temp++) /* S cycles from here on */
    if (BIT(temp)) { /* save this register */
       address += 4 ;
       ARMul_StoreWordS(state,address,state->Reg[temp]) ;
       if (state->abortSig && !state->Aborted)
             state->Aborted = ARMul_DataAbortV ;
       }

 if (state->Mode != USER26MODE && state->Mode != USER32MODE)
    (void)ARMul_SwitchMode(state,USER26MODE,state->Mode) ; /* restore the correct bank */

 if (state->Aborted) {
    TAKEABORT ;
    }
}

/***************************************************************************\
* This function does the work of adding two 32bit values together, and      *
* calculating if a carry has occurred.                                      *
\***************************************************************************/

static ARMword Add32(ARMword a1,ARMword a2,int *carry)
{
  ARMword result = (a1 + a2);
  unsigned int uresult = (unsigned int)result;
  unsigned int ua1 = (unsigned int)a1;

  /* If (result == RdLo) and (state->Reg[nRdLo] == 0),
     or (result > RdLo) then we have no carry: */
  if ((uresult == ua1) ? (a2 != 0) : (uresult < ua1))
   *carry = 1;
  else
   *carry = 0;

  return(result);
}

/***************************************************************************\
* This function does the work of multiplying two 32bit values to give a     *
* 64bit result.                                                             *
\***************************************************************************/

static unsigned Multiply64(ARMul_State *state,ARMword instr,int msigned,int scc)
{
  int nRdHi, nRdLo, nRs, nRm; /* operand register numbers */
  ARMword RdHi, RdLo, Rm;
  int scount; /* cycle count */

  nRdHi = BITS(16,19);
  nRdLo = BITS(12,15);
  nRs = BITS(8,11);
  nRm = BITS(0,3);

  /* Needed to calculate the cycle count: */
  Rm = state->Reg[nRm];

  /* Check for illegal operand combinations first: */
  if (   nRdHi != 15
      && nRdLo != 15
      && nRs   != 15
      && nRm   != 15
      && nRdHi != nRdLo
      && nRdHi != nRm
      && nRdLo != nRm)
    {
      ARMword lo, mid1, mid2, hi; /* intermediate results */
      int carry;
      ARMword Rs = state->Reg[ nRs ];
      int sign = 0;

      if (msigned)
	{
	  /* Compute sign of result and adjust operands if necessary.  */
	  
	  sign = (Rm ^ Rs) & 0x80000000;
	  
	  if (((signed long)Rm) < 0)
	    Rm = -Rm;
	  
	  if (((signed long)Rs) < 0)
	    Rs = -Rs;
	}
      
      /* We can split the 32x32 into four 16x16 operations. This ensures
	 that we do not lose precision on 32bit only hosts: */
      lo =   ((Rs & 0xFFFF) * (Rm & 0xFFFF));
      mid1 = ((Rs & 0xFFFF) * ((Rm >> 16) & 0xFFFF));
      mid2 = (((Rs >> 16) & 0xFFFF) * (Rm & 0xFFFF));
      hi =   (((Rs >> 16) & 0xFFFF) * ((Rm >> 16) & 0xFFFF));
      
      /* We now need to add all of these results together, taking care
	 to propogate the carries from the additions: */
      RdLo = Add32(lo,(mid1 << 16),&carry);
      RdHi = carry;
      RdLo = Add32(RdLo,(mid2 << 16),&carry);
      RdHi += (carry + ((mid1 >> 16) & 0xFFFF) + ((mid2 >> 16) & 0xFFFF) + hi);

      if (sign)
	{
	  /* Negate result if necessary.  */
	  
	  RdLo = ~ RdLo;
	  RdHi = ~ RdHi;
	  if (RdLo == 0xFFFFFFFF)
	    {
	      RdLo = 0;
	      RdHi += 1;
	    }
	  else
	    RdLo += 1;
	}
      
      state->Reg[nRdLo] = RdLo;
      state->Reg[nRdHi] = RdHi;
      
    } /* else undefined result */
  else fprintf (stderr, "MULTIPLY64 - INVALID ARGUMENTS\n");
  
  if (scc)
    {
      if ((RdHi == 0) && (RdLo == 0))
	ARMul_NegZero(state,RdHi); /* zero value */
      else
	ARMul_NegZero(state,scc); /* non-zero value */
    }
  
  /* The cycle count depends on whether the instruction is a signed or
     unsigned multiply, and what bits are clear in the multiplier: */
  if (msigned && (Rm & ((unsigned)1 << 31)))
    Rm = ~Rm; /* invert the bits to make the check against zero */
  
  if ((Rm & 0xFFFFFF00) == 0)
    scount = 1 ;
  else if ((Rm & 0xFFFF0000) == 0)
    scount = 2 ;
  else if ((Rm & 0xFF000000) == 0)
    scount = 3 ;
  else
    scount = 4 ;
  
  return 2 + scount ;
}

/***************************************************************************\
* This function does the work of multiplying two 32bit values and adding    *
* a 64bit value to give a 64bit result.                                     *
\***************************************************************************/

static unsigned MultiplyAdd64(ARMul_State *state,ARMword instr,int msigned,int scc)
{
  unsigned scount;
  ARMword RdLo, RdHi;
  int nRdHi, nRdLo;
  int carry = 0;

  nRdHi = BITS(16,19);
  nRdLo = BITS(12,15);

  RdHi = state->Reg[nRdHi] ;
  RdLo = state->Reg[nRdLo] ;

  scount = Multiply64(state,instr,msigned,LDEFAULT);

  RdLo = Add32(RdLo,state->Reg[nRdLo],&carry);
  RdHi = (RdHi + state->Reg[nRdHi]) + carry;

  state->Reg[nRdLo] = RdLo;
  state->Reg[nRdHi] = RdHi;

  if (scc) {
    if ((RdHi == 0) && (RdLo == 0))
     ARMul_NegZero(state,RdHi); /* zero value */
    else
     ARMul_NegZero(state,scc); /* non-zero value */
  }

  return scount + 1; /* extra cycle for addition */
}
