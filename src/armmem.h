/*
    armmem.c - Memory map decoding, ROM and RAM emulation.
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

#ifndef _ARMMEM_H_
#define _ARMMEM_H_


#define DRAM_BITS       (23)                    /* 8MB of DRAM */
#define ROM_BANKS	(1)
#define ROM_BITS	(28)			/* 0x10000000 each bank */

typedef struct mem_state_t {
	ARMword *	dram;
	ARMword *	rom[ROM_BANKS];
	long		rom_size[ROM_BANKS];
} mem_state_t;

void	mem_reset(ARMul_State *state);
ARMword	mem_read_word(ARMul_State *state, ARMword addr);
void	mem_write_word(ARMul_State *state, ARMword addr, ARMword data);
void	dump_dram(ARMul_State *state);


#endif	/* _ARMMEM_H_ */

