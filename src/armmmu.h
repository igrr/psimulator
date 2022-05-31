/*
    armmmu.c - Memory Management Unit emulation.
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

#ifndef _ARMMMU_H_
#define _ARMMMU_H_


/* The MMU is accessible with MCR and MRC operations to copro 15: */

#define MMU_COPRO			(15)

/* Register numbers in the MMU: */

typedef enum mmu_regnum_t {
	MMU_ID				= 0,
	MMU_CONTROL			= 1,
	MMU_TRANSLATION_TABLE_BASE	= 2,
	MMU_DOMAIN_ACCESS_CONTROL	= 3,
	MMU_FAULT_STATUS		= 5,
	MMU_FAULT_ADDRESS		= 6,
#ifdef MMU_V4
	MMU_V4_CACHE_OPS		= 7,
	MMU_V4_TLB_OPS			= 8
#else
	MMU_V3_FLUSH_TLB		= 5,
	MMU_V3_FLUSH_TLB_ENTRY		= 6,
	MMU_V3_FLUSH_CACHE		= 7,
#endif
} mmu_regnum_t;


/* Bits in the control register */

#define CONTROL_MMU			(1<<0)
#define CONTROL_ALIGN_FAULT		(1<<1)
#define CONTROL_CACHE			(1<<2)
#define CONTROL_WRITE_BUFFER		(1<<3)
#define CONTROL_BIG_ENDIAN		(1<<7)
#define CONTROL_SYSTEM			(1<<8)
#define CONTROL_ROM			(1<<9)


/* Sizes of the cache and the Translation Lookaside Buffer: */

#define CACHE_SIZE	(8 * 1024)			/* 8 Kbyte cache */
#define CACHE_BANKS	(4)				/* 4-way set assoc */
#define CACHE_LINES	(CACHE_SIZE / CACHE_BANKS / 16)	/* 4 words per line */
#define TLB_ENTRIES	(64)


/* The tag field of cache_line_t contains the 28-bit address
   of the quad-word of data, and a valid flag bit. */

#define TAG_ADDR_MASK	(0xFFFFFFF0)
#define TAG_VALID_FLAG	(0x00000001)

typedef struct cache_line_t {
	ARMword		data[4];
	ARMword		tag;
} cache_line_t;


typedef enum tlb_mapping_t {
	TLB_INVALID = 0,
	TLB_SMALLPAGE = 1,
	TLB_LARGEPAGE = 2,
	TLB_SECTION = 3
} tlb_mapping_t;

/* Permissions bits in a TLB entry:
 *
 *		 31         12 11 10  9  8  7  6  5  4  3   2   1   0
 *		+-------------+-----+-----+-----+-----+---+---+-------+
 * Page:	|             | ap3 | ap2 | ap1 | ap0 | C | B |       |
 *		+-------------+-----+-----+-----+-----+---+---+-------+
 *
 *		 31         12 11 10  9              4  3   2   1   0
 *		+-------------+-----+-----------------+---+---+-------+
 * Section:	|             |  AP |                 | C | B |       |
 *		+-------------+-----+-----------------+---+---+-------+
 */


typedef struct tlb_entry_t {
	ARMword		virt_addr;
	ARMword		phys_addr;
	ARMword		perms;
	ARMword		domain;
	tlb_mapping_t	mapping;
} tlb_entry_t;


/*
section:
	section base address	[31:20]
	AP			- table 8-2, page 8-8
	domain
	C,B

page:
	page base address	[31:16] or [31:12]
	ap[3:0]
	domain (from L1)
	C,B
*/


typedef struct mmu_state_t {
	ARMword		control;
	ARMword		translation_table_base;
	ARMword		domain_access_control;
	ARMword		fault_status;
	ARMword		fault_address;
	cache_line_t	cache[CACHE_LINES][CACHE_BANKS];
	tlb_entry_t	tlb[TLB_ENTRIES];
	int		tlb_cycle;
	ARMword		last_domain;

} mmu_state_t;


/* FS[3:0] in the fault status register: */

typedef enum fault_t {
	NO_FAULT 			= 0x0,
	ALIGNMENT_FAULT			= 0x1,
	SECTION_TRANSLATION_FAULT	= 0x5,
	PAGE_TRANSLATION_FAULT		= 0x7,
	SECTION_DOMAIN_FAULT		= 0x9,
	PAGE_DOMAIN_FAULT		= 0xB,
	SECTION_PERMISSION_FAULT	= 0xD,
	SUBPAGE_PERMISSION_FAULT	= 0xF
} fault_t;


void		mmu_reset(ARMul_State *state);

fault_t 	mmu_read_word(ARMul_State *state, ARMword virt_addr, ARMword *data);
fault_t		mmu_write_word(ARMul_State *state, ARMword virt_addr, ARMword data);

ARMword		mmu_mrc(ARMul_State *state, ARMword instr);
void		mmu_mcr(ARMul_State *state, ARMword instr, ARMword value);

void		mmu_tlb_invalidate_all(ARMul_State *state);
void		mmu_tlb_invalidate_entry(ARMul_State *state, ARMword addr);
tlb_entry_t *	mmu_tlb_search(ARMul_State *state, ARMword virt_addr);

void		mmu_cache_invalidate(ARMul_State *state);
cache_line_t *	mmu_cache_search(ARMul_State *state, ARMword addr);
cache_line_t *	mmu_cache_alloc(ARMul_State *state, ARMword addr);


#endif	/* _ARMMMU_H_ */

