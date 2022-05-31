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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "armdefs.h"

#define sram_read_word	_read_word
#define sram_write_word	_write_word
#define boot_read_word	_read_word
#define boot_write_word	_write_word

ARMword	_read_word(ARMul_State *state, ARMword addr);
void	_write_word(ARMul_State *state, ARMword addr, ARMword data);
ARMword	dram_read_word(ARMul_State *state, ARMword addr);
void	dram_write_word(ARMul_State *state, ARMword addr, ARMword data);
ARMword	io_read_word(ARMul_State *state, ARMword addr);
void	io_write_word(ARMul_State *state, ARMword addr, ARMword data);
ARMword	rom_read_word(ARMul_State *state, ARMword addr);


typedef struct mem_bank_t {
	ARMword	(*read_word)(ARMul_State *state, ARMword addr);
	void	(*write_word)(ARMul_State *state, ARMword addr, ARMword data);
} mem_bank_t;


/* Memory map for the CL-PS7111 */

static mem_bank_t mem_banks[16] = {
	{ rom_read_word,	_write_word },		/* 0x00000000 */
	{ _read_word,		_write_word },		/* 0x10000000 */
	{ _read_word,		_write_word },		/* 0x20000000 */
	{ _read_word,		_write_word },		/* 0x30000000 */
	{ _read_word,		_write_word },		/* 0x40000000 */
	{ _read_word,		_write_word },		/* 0x50000000 */
	{ sram_read_word,	sram_write_word },	/* 0x60000000 */
	{ boot_read_word,	boot_write_word },	/* 0x70000000 */
	{ io_read_word,		io_write_word },	/* 0x80000000 */
	{ _read_word,		_write_word },		/* 0x90000000 */
	{ _read_word,		_write_word },		/* 0xA0000000 */
	{ _read_word,		_write_word },		/* 0xB0000000 */
	{ dram_read_word,	dram_write_word },	/* 0xC0000000 */
	{ dram_read_word,	dram_write_word },	/* 0xD0000000 */
	{ _read_word,		_write_word },		/* 0xE0000000 */
	{ _read_word,		_write_word }		/* 0xF0000000 */
};

const char *rom_filenames[ROM_BANKS] = {"./bootsim.rom"};


void
mem_reset(ARMul_State *state)
{
	FILE *f;
	char *p;
	long s;
	int bank;

	free(state->mem.dram);
	state->mem.dram = calloc(1, 1 << DRAM_BITS);
	if (!state->mem.dram) {
		fprintf(stderr, "Couldn't allocate memory for dram\n");
		exit(1);
	}
	for (bank = 0; bank < ROM_BANKS; bank++) {
		free(state->mem.rom[bank]);
		state->mem.rom[bank] = NULL;
		state->mem.rom_size[bank] = 0;
		f = fopen(rom_filenames[bank], "r");
		if (!f) {
			perror(rom_filenames[bank]);
			fprintf(stderr, "Couldn't open boot ROM %s\n", rom_filenames[bank]);
			exit(1);
		}
		if (fseek(f, 0L, SEEK_END)) {
			fprintf(stderr, "Couldn't seek to end of rom file\n");
			exit(1);
		}
		state->mem.rom_size[bank] = ftell(f);
		state->mem.rom[bank] = malloc(state->mem.rom_size[bank]);
		if (!state->mem.rom[bank]) {
			fprintf(stderr, "Couldn't allocate memory for rom\n");
			exit(1);
		}
		rewind(f);
		p = (char *)state->mem.rom[bank];
		s = state->mem.rom_size[bank];
		while (!feof(f) && (s > 0)) {
			size_t r;
			
			r = fread(p, 1, s, f);
			p += r;
			s -= r;
			if (ferror(f)) {
				perror(rom_filenames[bank]);
				exit(1);
			}
		}
		fclose(f);
	}
}

ARMword
mem_read_word(ARMul_State *state, ARMword addr)
{
	return mem_banks[addr >> 28].read_word(state, addr);
}

void
mem_write_word(ARMul_State *state, ARMword addr, ARMword data)
{
	mem_banks[addr >> 28].write_word(state, addr, data);
}


/* Accesses that map to gaps in the memory map go here: */

ARMword
_read_word(ARMul_State *state, ARMword addr)
{
	return 0xFFFFFFFF;
}

void
_write_word(ARMul_State *state, ARMword addr, ARMword data)
{
}


/* Physical DRAM is aliased throughout the DRAM bank, 
   i.e. the high address bits are not decoded. */

ARMword
dram_read_word(ARMul_State *state, ARMword addr)
{
	ARMword data = 0xFFFFFFFF;
        if(IS_ADDR_VALID(addr))	{
		data = state->mem.dram[__phys_to_virt(addr) >> 2];
	} else {
		fprintf(stderr, "Bad memory read! ADDR=0x%08lx pc=0x%08lx lr=0x%08lX\n", addr, state->Reg[15], state->Reg[14]-4);
	}
	return data;
}

void
dram_write_word(ARMul_State *state, ARMword addr, ARMword data)
{
        if(IS_ADDR_VALID(addr))	{
		state->mem.dram[__phys_to_virt(addr) >> 2] = data;
		if (addr < state->io.lcd_limit) {
			lcd_write(state, addr, data);
		}
	}
	else
	{
		fprintf(stderr, "Bad memory write! ADDR=0x%08lx pc=0x%08lx lr=0x%08lX\n", addr, state->Reg[15], state->Reg[14]-4);
	}
}


ARMword
rom_read_word(ARMul_State *state, ARMword addr)
{
	ARMword data;
	int bank;
	ARMword offset;
	
	bank = addr >> ROM_BITS;
	offset = addr & (~0 >> ROM_BITS);
	if (offset < state->mem.rom_size[bank]) {
		unsigned char *p;
		
		/* The rom file is in target-endian order, but we address
		   it as an array of words on the host system.  Make sure that
		   we hand words to the target in target-endian order: */
		p = (unsigned char *)&(state->mem.rom[bank][offset >> 2]);
		data = ((ARMword)p[0]) | ((ARMword)p[1] << 8) |
			((ARMword)p[2] << 16) | ((ARMword)p[3] << 24);
	}
	return data;
}

void dump_dram(ARMul_State *state)
{FILE *f;
	if(state)
	{
		f = fopen("psion_RAM.bin", "w");
		if(f)
		{
			fwrite(state->mem.dram, 1, 1 << DRAM_BITS, f);
			fflush(f);
			fclose(f);
		}
	}
}