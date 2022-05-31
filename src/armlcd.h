/*
    armlcd.c - LCD display emulation in an X window.
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

#ifndef _ARMLCD_H_
#define _ARMLCD_H_


void	lcd_enable(ARMul_State *state, int width, int height, int depth);
void	lcd_disable(ARMul_State *state);
void	lcd_write(ARMul_State *state, ARMword addr, ARMword data);
void	lcd_cycle(ARMul_State *state);
void	lcd_dirty(ARMul_State *state, ARMword addr);
void	lcd_clean(ARMul_State *state);

#endif	/* _ARMLCD_H_ */

