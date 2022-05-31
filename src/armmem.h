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

/*
 *  Psion Series 5 physical memory map
 *
 *  0xc0000000 size 512k
 *  0xc0100000 size 512k
 *  0xc0400000 size 512k
 *  0xc0500000 size 512k
 *  0xc1000000 size 512k
 *  0xc1100000 size 512k
 *  0xc1400000 size 512k
 *  0xc1500000 size 512k
 *  0xd0000000 size 512k
 *  0xd0100000 size 512k
 *  0xd0400000 size 512k
 *  0xd0500000 size 512k
 *  0xd1000000 size 512k
 *  0xd1100000 size 512k
 *  0xd1400000 size 512k
 *  0xd1500000 size 512k
 *
 *  phys addr bits: 110n 000n 0n0n 0xxx xxxx xxxx xxxx xxxx
 *  dram addr bits: 1100 0000 0nnn nxxx xxxx xxxx xxxx xxxx
 */

#define IS_ADDR_VALID(x)        (!((x) & 0x2ea80000))

#define __virt_to_phys(x)	(((x) & 0x0007ffff) | \
				(((x) & 0x00400000) << 6) | \
				(((x) & 0x00200000) << 3) | \
				(((x) & 0x00100000) << 2) | \
				(((x) & 0x00080000) << 1) | \
				0xc0000000)

#define __phys_to_virt(x)	(((x) & 0x0007ffff) | \
				(((x) & 0x10000000) >> 6) | \
				(((x) & 0x01000000) >> 3) | \
				(((x) & 0x00400000) >> 2) | \
				(((x) & 0x00100000) >> 1))



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

