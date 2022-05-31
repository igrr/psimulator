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

#include <assert.h>
#include <string.h>
#include "armdefs.h"


ARMword tlb_masks[4] = {
	0x00000000,		/* TLB_INVALID */
	0xFFFFF000,		/* TLB_SMALLPAGE */
	0xFFFF0000,		/* TLB_LARGEPAGE */
	0xFFF00000		/* TLB_SECTION */
};
	


void
mmu_reset(ARMul_State *state)
{
	state->mmu.control = 0x70;
	state->mmu.translation_table_base = 0xDEADC0DE;
	state->mmu.domain_access_control = 0xDEADC0DE;
	state->mmu.fault_status = 0;
	state->mmu.fault_address = 0;
	mmu_cache_invalidate(state);
	mmu_tlb_invalidate_all(state);
}


/* This function encodes table 8-2 Interpreting AP bits,
   returning non-zero if access is allowed. */
static int
check_perms(ARMul_State *state, int ap, int read)
{
	int s, r, user;
	
	s = state->mmu.control & CONTROL_SYSTEM;
	r = state->mmu.control & CONTROL_ROM;
	user = (state->Mode == USER32MODE) || (state->Mode == USER26MODE);
	
	switch (ap) {
	case 0:
		return read && ((s && !user) || r);
	case 1:
		return !user;
	case 2:
		return read || !user;
	case 3:
		return 1;
	}
	return 0;
}

static fault_t
check_access(ARMul_State *state, ARMword virt_addr, tlb_entry_t *tlb, int read)
{
	int access;
	
	state->mmu.last_domain = tlb->domain;
	access = (state->mmu.domain_access_control >> (tlb->domain * 2)) & 3;
	if ((access == 0) || (access == 2)) {
		/* It's unclear from the documentation whether this
		   should always raise a section domain fault, or if
		   it should be a page domain fault in the case of an
		   L1 that describes a page table.  In the ARM710T
		   datasheets, "Figure 8-9: Sequence for checking faults"
		   seems to indicate the former, while "Table 8-4: Priority
		   encoding of fault status" gives a value for FS[3210] in
		   the event of a domain fault for a page.  Hmm. */
		return SECTION_DOMAIN_FAULT;
	}
	if (access == 1) {
		/* client access - check perms */
		int subpage, ap;
		
		switch (tlb->mapping) {
		case TLB_SMALLPAGE:
			subpage = (virt_addr >> 10) & 3;
			break;
		case TLB_LARGEPAGE:
			subpage = (virt_addr >> 14) & 3;
			break;
		case TLB_SECTION:
			subpage = 3;
			break;
		default:
			assert(0);
			subpage = 0;	/* cleans a warning */
		}
		ap = (tlb->perms >> (subpage * 2 + 4)) & 3;
		if (!check_perms(state, ap, read)) {
			if (tlb->mapping == TLB_SECTION) {
				return SECTION_PERMISSION_FAULT;
			} else {
				return SUBPAGE_PERMISSION_FAULT;
			}
		}
	} else {/* access == 3 */
		/* manager access - don't check perms */
	}
	return NO_FAULT;
}

static fault_t
translate(ARMul_State *state, ARMword virt_addr, tlb_entry_t **tlb)
{
	*tlb = mmu_tlb_search(state, virt_addr);
	if (!*tlb) {
		/* walk the translation tables */
		ARMword l1addr, l1desc;
		tlb_entry_t entry;
		
		l1addr = state->mmu.translation_table_base & 0xFFFFC000;
		l1addr = (l1addr | (virt_addr >> 18)) & ~3;
		l1desc = mem_read_word(state, l1addr);
		switch (l1desc & 3) {
		case 0:
		case 3:
			return SECTION_TRANSLATION_FAULT;
		case 1:
			/* page table */
			{
				ARMword l2addr, l2desc;
				
				l2addr = l1desc & 0xFFFFFC00;
				l2addr = (l2addr | ((virt_addr & 0x000FF000) >> 10)) & ~3;
				l2desc = mem_read_word(state, l2addr);
				
				entry.virt_addr = virt_addr;
				entry.phys_addr = l2desc;
				entry.perms = l2desc & 0x00000FFC;
				entry.domain = (l1desc >> 5) & 0x0000000F;
				switch (l2desc & 3) {
				case 0:
				case 3:
					state->mmu.last_domain = entry.domain;
					return PAGE_TRANSLATION_FAULT;
				case 1:
					entry.mapping = TLB_LARGEPAGE;
					break;
				case 2:
					entry.mapping = TLB_SMALLPAGE;
					break;
				}
			}
			break;
		case 2:
			/* section */
			entry.virt_addr = virt_addr;
			entry.phys_addr = l1desc;
			entry.perms = l1desc & 0x00000C0C;
			entry.domain = (l1desc >> 5) & 0x0000000F;
			entry.mapping = TLB_SECTION;
			break;
		}
		entry.virt_addr &= tlb_masks[entry.mapping];
		entry.phys_addr &= tlb_masks[entry.mapping];
		
		/* place entry in the tlb */
		*tlb = &(state->mmu.tlb[state->mmu.tlb_cycle]);
		state->mmu.tlb_cycle = (state->mmu.tlb_cycle + 1) % TLB_ENTRIES;
		**tlb = entry;
	}
	state->mmu.last_domain = (*tlb)->domain;
	return NO_FAULT;
}

#if 0
/* XXX */
int hack = 0;
#endif

fault_t
mmu_read_word(ARMul_State *state, ARMword virt_addr, ARMword *data)
{
	tlb_entry_t *tlb;
	ARMword phys_addr;
	fault_t fault;
	
	if (!(state->mmu.control & CONTROL_MMU)) {
		*data = mem_read_word(state, virt_addr);
		return NO_FAULT;
	}

#if 0
/* XXX */
	if (hack && (virt_addr >= 0xc0000000) && (virt_addr < 0xc0200000)) {
		printf("0x%08x\n", virt_addr);
	}
#endif
	
	if ((virt_addr & 3) && (state->mmu.control & CONTROL_ALIGN_FAULT)) {
		return ALIGNMENT_FAULT;
	}
	if (state->mmu.control & CONTROL_CACHE) {
		cache_line_t *cache;

		cache = mmu_cache_search(state, virt_addr);
		if (cache) {
			*data = cache->data[(virt_addr >> 2) & 3];
			return NO_FAULT;
		}
	}
	fault = translate(state, virt_addr, &tlb);
	if (fault) {
		return fault;
	}
	fault = check_access(state, virt_addr, tlb, 1);
	if (fault) {
		return fault;
	}
	
	phys_addr = (tlb->phys_addr & tlb_masks[tlb->mapping]) |
			(virt_addr & ~tlb_masks[tlb->mapping]);
	
	/* allocate to the cache if cacheable */
	if ((tlb->perms & 0x08) && (state->mmu.control & CONTROL_CACHE)) {
		cache_line_t *cache;
		ARMword fetch;
		int i;
		
		cache = mmu_cache_alloc(state, virt_addr);
		fetch = phys_addr & 0xFFFFFFF0;
		for (i = 0; i < 4; i++) {
			cache->data[i] = mem_read_word(state, fetch);
			fetch += 4;
		}
		cache->tag = (virt_addr & TAG_ADDR_MASK) | TAG_VALID_FLAG;
		*data = cache->data[(virt_addr >> 2) & 3];
		return NO_FAULT;
	} else {
		*data = mem_read_word(state, phys_addr);
		return NO_FAULT;
	}
}


fault_t
mmu_write_word(ARMul_State *state, ARMword virt_addr, ARMword data)
{
	tlb_entry_t *tlb;
	ARMword phys_addr;
	fault_t fault;
	
	if (!(state->mmu.control & CONTROL_MMU)) {
		mem_write_word(state, virt_addr, data);
		return NO_FAULT;
	}

	if ((virt_addr & 3) && (state->mmu.control & CONTROL_ALIGN_FAULT)) {
		return ALIGNMENT_FAULT;
	}
	if (state->mmu.control & CONTROL_CACHE) {
		cache_line_t *cache;

		cache = mmu_cache_search(state, virt_addr);
		if (cache) {
			cache->data[(virt_addr >> 2) & 3] = data;
		}
	}
	fault = translate(state, virt_addr, &tlb);
	if (fault) {
		return fault;
	}
	fault = check_access(state, virt_addr, tlb, 0);
	if (fault) {
		return fault;
	}
	
	phys_addr = (tlb->phys_addr & tlb_masks[tlb->mapping]) |
			(virt_addr & ~tlb_masks[tlb->mapping]);
	
	mem_write_word(state, phys_addr, data);
	return NO_FAULT;
}


ARMword
mmu_mrc(ARMul_State *state, ARMword instr)
{
	mmu_regnum_t creg = BITS(16, 19) & 15;
	ARMword data;
	
	switch (creg) {
	case MMU_ID:
//		printf("mmu_mrc read ID     ");
#ifdef MMU_V4
		data = 0x41018100;	/* v4 */
#else
		data = 0x41007100;	/* v3 */
#endif
		break;
	case MMU_CONTROL:
//		printf("mmu_mrc read CONTROL");
		data = state->mmu.control;
		break;
	case MMU_TRANSLATION_TABLE_BASE:
//		printf("mmu_mrc read TTB    ");
		data = state->mmu.translation_table_base;
		break;
	case MMU_DOMAIN_ACCESS_CONTROL:
//		printf("mmu_mrc read DACR   ");
		data = state->mmu.domain_access_control;
		break;
	case MMU_FAULT_STATUS:
//		printf("mmu_mrc read FSR    ");
		data = state->mmu.fault_status;
		break;
	case MMU_FAULT_ADDRESS:
//		printf("mmu_mrc read FAR    ");
		data = state->mmu.fault_address;
		break;
	default:
		printf("mmu_mrc read UNKNOWN - reg %d\n", creg);
		data = 0;
		break;
	}
//	printf("\t\t\t\t\tpc = 0x%08x\n", state->Reg[15]);
	return data;
}


void
mmu_mcr(ARMul_State *state, ARMword instr, ARMword value)
{
	mmu_regnum_t creg = BITS(16, 19) & 15;
	switch (creg) {
	case MMU_CONTROL:
		state->mmu.control = (value | 0x70) & 0x3FF;
//		fprintf(stderr,"mmu_mcr wrote CONTROL      %08x\n",state->mmu.control);
		break;
	case MMU_TRANSLATION_TABLE_BASE:
		state->mmu.translation_table_base = value & 0xFFFFC000;
//		fprintf(stderr,"mmu_mcr wrote TTB          %08x\n",state->mmu.translation_table_base);
		break;
	case MMU_DOMAIN_ACCESS_CONTROL:
//		printf("mmu_mcr wrote DACR         ");
		state->mmu.domain_access_control = value;
		break;
#ifdef MMU_V4
	case MMU_FAULT_STATUS:
		state->mmu.fault_status = value & 0xFF;
		break;
	case MMU_FAULT_ADDRESS:
		state->mmu.fault_address = value;
		break;
	case MMU_V4_CACHE_OPS:			/* incomplete */
		if ((BITS(5, 7) & 7) == 0) {
			mmu_cache_invalidate(state);
		}
		break;
	case MMU_V4_TLB_OPS:			/* incomplete */
		switch (BITS(5, 7) & 7) {
		case 0:
			mmu_tlb_invalidate_all(state);
			break;
		case 1:
			mmu_tlb_invalidate_entry(state, value);
			break;
		}
		break;
#else
	case MMU_V3_FLUSH_TLB:
//		printf("mmu_mcr wrote FLUSH_TLB    ");
		mmu_tlb_invalidate_all(state);
		break;
	case MMU_V3_FLUSH_TLB_ENTRY:
//		printf("mmu_mcr wrote FLUSH_TLB_ENTRY");
		mmu_tlb_invalidate_entry(state, value);
		break;
	case MMU_V3_FLUSH_CACHE:
//		printf("mmu_mcr wrote FLUSH_CACHE    ");
		mmu_cache_invalidate(state);
		break;
#endif
	default:
		printf("mmu_mcr wrote UNKNOWN - reg %d\n", creg);
		break;
	}
//	printf("\t\t\t\tpc = 0x%08x\n", state->Reg[15]);
}


void
mmu_tlb_invalidate_all(ARMul_State *state)
{
	int entry;
	
	for (entry = 0; entry < TLB_ENTRIES; entry++) {
		state->mmu.tlb[entry].mapping = TLB_INVALID;
	}
	state->mmu.tlb_cycle = 0;
}

void
mmu_tlb_invalidate_entry(ARMul_State *state, ARMword addr)
{
	tlb_entry_t *tlb;
	
	tlb = mmu_tlb_search(state, addr);
	if (tlb) {
		tlb->mapping = TLB_INVALID;
	}
}

tlb_entry_t *
mmu_tlb_search(ARMul_State *state, ARMword virt_addr)
{
	int entry;
	
	for (entry = 0; entry < TLB_ENTRIES; entry++) {
		tlb_entry_t *tlb;
		ARMword mask;

		tlb = &(state->mmu.tlb[entry]);
		if (tlb->mapping == TLB_INVALID) {
			continue;
		}
		mask = tlb_masks[tlb->mapping];
		if ((virt_addr & mask) == (tlb->virt_addr & mask)) {
			return tlb;
		}
	}
	return NULL;
}


void
mmu_cache_invalidate(ARMul_State *state)
{
	memset(state->mmu.cache, 0,
		CACHE_LINES * CACHE_BANKS * sizeof(cache_line_t));
}

cache_line_t *
mmu_cache_search(ARMul_State *state, ARMword addr)
{
	int bank, line;
	ARMword match;
	cache_line_t *cache;
	
	line = (addr >> 4) & (CACHE_LINES - 1);
	match = (addr & TAG_ADDR_MASK) | TAG_VALID_FLAG;
	cache = state->mmu.cache[line];
	for (bank = 0; bank < CACHE_BANKS; bank++) {
		if (cache->tag == match) {
			return cache;
		}
		cache++;
	}
	return NULL;
}

cache_line_t *
mmu_cache_alloc(ARMul_State *state, ARMword addr)
{
	int bank, line;
	cache_line_t *cache;
	
	line = (addr >> 4) & (CACHE_LINES - 1);
	cache = state->mmu.cache[line];
	for (bank = 0; bank < CACHE_BANKS; bank++) {
		if (!(cache->tag & TAG_VALID_FLAG)) {
			return cache;
		}
		cache++;
	}
	cache -= CACHE_BANKS;		/* back to bank 0 */
	cache += rand() % CACHE_BANKS;	/* choose a random bank */
	return cache;
}


