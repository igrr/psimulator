/*
    armio.c - I/O registers and interrupt controller.
    ARMulator extensions for the ARM7100 family.
    Copyright (C) 1999  Ben Williamson

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <unistd.h>

#include "armdefs.h"
#include "clps7110.h"

#define TC_DIVISOR	(9000)	/* Set your BogoMips here :) */

unsigned char keyboard[8];

static void update_int(ARMul_State *state)
{
	ARMword	requests = state->io.intsr & state->io.intmr;
	
	state->NfiqSig = (requests & 0x000f) ? LOW : HIGH;
	state->NirqSig = (requests & 0xfff0) ? LOW : HIGH;
}


static void update_lcd(ARMul_State *state)
{
	lcd_disable(state);
	if (state->io.syscon & LCDEN) {
		ARMword lcdcon = state->io.lcdcon;
		ARMword vbufsiz = lcdcon & VBUFSIZ;
		ARMword linelen = (lcdcon & LINELEN) >> LINELEN_SHIFT;
		int width, height, depth;

		switch (lcdcon & (GSEN|GSMD)) {
		case GSEN:
			depth = 2;
			break;
		case GSEN|GSMD:
			depth = 4;
			break;
		default:
			depth = 1;
			break;
		}
		width = (linelen + 1) * 16;
		height = (vbufsiz + 1) * 128 / depth / width;
		lcd_enable(state, width, height, depth);
	}
}


void
io_reset(ARMul_State *state)
{
	state->io.syscon = 0;
	state->io.sysflg = URXFE;
	state->io.intmr = 0;
	state->io.intsr = UTXINT;	/* always ready to transmit */
	state->io.tcd[0] = 0xffff;
	state->io.tcd[1] = 0xffff;
	state->io.tcd_reload[0] = 0xffff;
	state->io.tcd_reload[1] = 0xffff;
	state->io.tc_prescale = TC_DIVISOR;
	state->io.uartdr = 0;
	state->io.lcdcon = 0;
	state->io.pallsw = 0x000000F0;
	state->io.palmsw = 0;
	state->io.lcd_limit = 0;
	state->Exception = TRUE;
}


void
io_do_cycle(ARMul_State *state)
{
	int t;

	/* timer/counters */
	state->io.tc_prescale--;
	if (state->io.tc_prescale < 0) {
		state->io.tc_prescale = TC_DIVISOR;
		/* decrement the timer/counters, interrupt on underflow */
		for (t = 0; t < 2; t++) {
			if (state->io.tcd[t] == 0) {
				/* underflow */
				if (state->io.syscon & (t ? TC2M : TC1M)) {
					/* prescale */
					state->io.tcd[t] = state->io.tcd_reload[t];
				} else {
					state->io.tcd[t] = 0xffff;
				}
				state->io.intsr |= (t ? TC2OI : TC1OI);
				update_int(state);
			} else {
				state->io.tcd[t]--;
			}
		}
		/* uart receive - do this at the timers'
		   prescaled rate for performance reasons */
		if (state->io.sysflg & URXFE) {
			char c;
			
			if (0 < read(0, &c, 1)) {
				state->io.uartdr = c;
				state->io.sysflg &= ~URXFE;
				state->io.intsr |= URXINT;
				update_int(state);
			}
		}
		/* keep the UI alive */
		lcd_cycle(state);
	}
}


/* Internal registers from 0x80000000 to 0x80002000.
   We also define a "debug I/O" register thereafter. */

ARMword
io_read_word(ARMul_State *state, ARMword addr)
{
	ARMword data = 0;

	switch (addr - 0x80000000) {
	case PADR:
		if(state->io.syscon & 8)
			data = keyboard[state->io.syscon & 7] & 0x7F;
		break;
//	case PBDR:
//	case PCDR:		*
//	case PDDR:		*
//	case PADDR:
//	case PBDDR:
//	case PCDDR:
//	case PDDDR:		*
//	case PEDR:
//	case PEDDR:
	case SYSCON:
		data = state->io.syscon;
		break;
	case SYSFLG:
		data = state->io.sysflg;
		break;
//	case MEMCFG1:
//	case MEMCFG2:
//	case /* DRFPR */:
	case INTSR:
		data = state->io.intsr;
		break;
	case INTMR:
		data = state->io.intmr;
		break;
	case LCDCON:
		data = state->io.lcdcon;
		break;
	case TC1D:
		data = state->io.tcd[0];
		break;
	case TC2D:
		data = state->io.tcd[1];
		break;
//	case RTCDR:
//	case RTCMR:
//	case /* PMPCON */:
//	case CODR:
	case UARTDR:
		data = state->io.uartdr;
		state->io.sysflg |= URXFE;
		state->io.intsr &= ~URXINT;
		update_int(state);
		break;
//	case UBRLCR:		*
	case SYNCIO:
		/* if we return zero here, the battery voltage calculation
		   results in a divide-by-zero that messes up the kernel */
		data = 1;
		break;
	case PALLSW:
		data = state->io.pallsw;
	case PALMSW:
		data = state->io.palmsw;
		break;
	/* write-only: */
	case STFCLR:
	case BLEOI:
	case MCEOI:
	case TEOI:
	case TC1EOI:
	case TC2EOI:
	case RTCEOI:
	case UMSEOI:
	case COEOI:
	case HALT:
	case STDBY:
		break;

	default:
//		printf("io_read_word(0x%08x) = 0x%08x\n", addr, data);
	}
	return data;
}

void
io_write_word(ARMul_State *state, ARMword addr, ARMword data)
{
	ARMword tmp;

	switch (addr - 0x80000000) {
//	case PADR:
//	case PBDR:
//	case PCDR:		*
//	case PDDR:		*
//	case PADDR:
//	case PBDDR:
//	case PCDDR:
//	case PDDDR:		*
//	case PEDR:
//	case PEDDR:
	case SYSCON:
		tmp = state->io.syscon;
		state->io.syscon = data;
		if ((tmp & LCDEN) != (data & LCDEN)) {
			update_lcd(state);
		}
//		printf("SYSCON = 0x%08x\n", data);
		break;
	case SYSFLG:
		break;
//	case MEMCFG1:
//	case MEMCFG2:
//	case /* DRFPR */:
	case INTSR:
		break;
	case INTMR:
		state->io.intmr = data;
		update_int(state);
//		printf("INTMR = 0x%08x\n", data);
		break;
	case LCDCON:
		tmp = state->io.lcdcon;
		state->io.lcdcon = data;
		if ((tmp & (VBUFSIZ|LINELEN|GSEN|GSMD)) != (data & (VBUFSIZ|LINELEN|GSEN|GSMD))) {
			update_lcd(state);
		}
		break;
	case TC1D:
		state->io.tcd[0] = state->io.tcd_reload[0] = data & 0xffff;
		break;
	case TC2D:
		state->io.tcd[1] = state->io.tcd_reload[1] = data & 0xffff;
		break;
//	case RTCDR:
//	case RTCMR:
//	case /* PMPCON */:
//	case CODR:
	case UARTDR:
		/* The UART writes chars to console */
		printf("%c", (char)data);
		fflush(stdout);
		break;
//	case UBRLCR:		*
//	case SYNCIO:		*
	case PALLSW:
		tmp = state->io.pallsw;
		state->io.pallsw = data;
		if(tmp != data) {
			update_lcd(state);
		}
	case PALMSW:
		tmp = state->io.palmsw;
		state->io.palmsw = data;
		if(tmp != data) {
			update_lcd(state);
		}
		break;
//	case STFCLR:
//	case BLEOI:
//	case MCEOI:
//	case TEOI:
	case TC1EOI:
		state->io.intsr &= ~TC1OI;
		update_int(state);
//		printf("TC1EOI\n");
		break;
	case TC2EOI:
		state->io.intsr &= ~TC2OI;
		update_int(state);
//		printf("TC2EOI\n");
		break;
//	case RTCEOI:
//	case UMSEOI:
//	case COEOI:
//	case HALT:
//	case STDBY:
	case 0x2000:
		/* Not a real register, for debugging only: */
		printf("io_write_word debug: 0x%08lx\n", data);
		break;
	default:
//		printf("io_write_word(0x%08x, 0x%08x)\n", addr, data);
	}
}


