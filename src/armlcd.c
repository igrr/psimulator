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

#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "armdefs.h"
#include "xkeycodes.h"

#define MAX_DEPTH	4		/* bits per pixel */
#define GREY_LEVELS	16
#define LCD_BASE	0xC0000000


static Display          *display = 0;
static Window           win = 0,root = 0;
static int              screen, sd, lcd_enabled = 0, img_size = 0, pixel_size = 0;
static GC               gc;
static Atom             wmDeleteWindow;
static XImage           *ximage = 0;
static Visual		*visual = 0;
static void             *xdata = 0;

static unsigned long color_32[GREY_LEVELS] = {
 0x00a7c57f, 0x009bb776, 0x0090aa6e, 0x00859d65,
 0x007a905d, 0x006f8354, 0x0064764c, 0x00596943,
 0x004d5b3b, 0x00424e32, 0x0037412a, 0x002c3421,
 0x00212719, 0x00161a10, 0x000b0d08, 0x00000000};
    
static unsigned long color[GREY_LEVELS] = {
 0x0000a62f, 0x00009dae, 0x0000954d, 0x000084ec,
 0x00007c8b, 0x00006c0a, 0x000063a9, 0x00005b48,
 0x00004ac7, 0x00004266, 0x00003205, 0x000029a4,
 0x00002123, 0x000010c2, 0x00000861, 0x00000000};

static int lcd_width, lcd_height, lcd_depth;

extern ARMul_State *state;
extern unsigned char keyboard[8];

static int keymap[256];

unsigned long cr2pv(unsigned long c)
{unsigned long value;
// for 565:
  value = ((c >> 19) & 0x0000001F) |
          ((c >> 5 ) & 0x000007E0) |
          ((c << 8 ) & 0x0000F800);
  return value;
}

void
lcd_cycle(ARMul_State *state)
{XEvent               report;
 int code, fd;

	if(!display || !win) return;
	while(XPending(display))
	{
		XNextEvent( display, &report );
		if( report.xany.window == win )
		{
			switch( report.type )
			{
				case ClientMessage:
					if (report.xclient.format == 32 && report.xclient.data.l[0] == wmDeleteWindow)
						XAutoRepeatOn(display);
						XFlush(display);
						kill(getpid(), SIGTERM);
				break;
				case ButtonPress:
					printf("Screen press: %d %d\n", report.xbutton.x, report.xbutton.y);
				break;

				case ButtonRelease:
				break;

				case MotionNotify:
				break;

				case FocusIn:
				break;

				case FocusOut:
				break;

				case KeyPress:
					printf("Key press: %#04x\n", report.xkey.keycode);
					code = keymap[report.xkey.keycode & 0xFF];
					if(code--) keyboard[code >> 3] |= (1 << (code & 7));
				break;

				case KeyRelease:
					printf("Key release: %#04x\n", report.xkey.keycode);
					code = keymap[report.xkey.keycode & 0xFF];
					if(code--) keyboard[code >> 3] &= ~(1 << (code & 7));
				break;

				case DestroyNotify:
				break;

				case GraphicsExpose:
				case Expose:
					if(lcd_enabled)
					{
						XPutImage( display, win, gc, ximage,
							   report.xexpose.x,
							   report.xexpose.y,
							   report.xexpose.x,
							   report.xexpose.y,
							   report.xexpose.width,
							   report.xexpose.height);
					}
					else
					{
						XSetForeground(display, gc, cr2pv(0x00808080) );
						XFillRectangle(display, win, gc,
							       report.xexpose.x,
							       report.xexpose.y,
							       report.xexpose.width,
							       report.xexpose.height);
					}
				break;
			}
		}
	}
}

void
lcd_enable(ARMul_State *state, int width, int height, int depth)
{
	int i;
	XSetWindowAttributes attr;
	XGCValues            values;
	XEvent               event;

	lcd_width = width;
	lcd_height = height;
	lcd_depth = depth;
	state->io.lcd_limit = LCD_BASE + (width * height * depth / 8);
	if(!win)
	{
		if ( (display=XOpenDisplay(NULL)) == NULL ) 
		{ 
			fprintf( stderr, "Armulator: cannot connect to X server %s\n", XDisplayName(NULL));
			exit( -1 ); 
		} 

		screen     = DefaultScreen( display );
		root       = RootWindow( display, screen );
		sd         = DefaultDepth( display, screen );
		visual     = DefaultVisual( display, screen );
		pixel_size = sd / 8;
		img_size   = width * height;
		xdata      = malloc(img_size * pixel_size + sizeof(unsigned long));
		if(!xdata)
		{ 
			fprintf( stderr, "Armulator: can't allocate memory\n");
			exit( -1 ); 
		} 


		attr.background_pixmap = None;
		attr.override_redirect = False;
		attr.backing_store     = Always;
		attr.save_under        = False;
		attr.event_mask        = ExposureMask | 
					 KeyPressMask | 
					 KeyReleaseMask |
					 ButtonPressMask |
					 ButtonReleaseMask |
					 FocusChangeMask |
					 StructureNotifyMask |
					 PointerMotionMask;

		win = XCreateWindow( display, root, 0, 0, width, height, 0, sd,
			 InputOutput,
			 CopyFromParent,
			 CWBackPixmap |
			 CWOverrideRedirect |
			 CWEventMask |
			 CWSaveUnder |
			 CWBackingStore,
			 &attr);

		{char                 *name = "Armulator";
		 XWMHints             wm_hints;
		 XSizeHints           size_hints;
		 XClassHint           class_hints;
		 XTextProperty        windowName;

			XStringListToTextProperty( &name, 1, &windowName );
			XSetWMName( display, win, &windowName );
			size_hints.flags       = PMinSize | USPosition; 
			size_hints.min_width   = width;
			size_hints.min_height  = height;
			XSetWMNormalHints( display, win, &size_hints );
			wm_hints.initial_state = NormalState;
			wm_hints.input         = True; 
			wm_hints.flags         = StateHint | InputHint;
			XSetWMHints( display, win, &wm_hints );
			class_hints.res_name   = name; 
			class_hints.res_class  = name; 
			XSetClassHint( display, win, &class_hints );
			wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
			XSetWMProtocols(display, win, &wmDeleteWindow, 1);
		}
		XMapWindow( display, win );
		gc = XCreateGC(display,win,0,&values);
		XSetGraphicsExposures(display, gc, True);
		XAutoRepeatOff(display);
		ximage = XCreateImage(display, visual, sd, ZPixmap, 0, (char*)xdata, width, height, 8, 0);
		XFlush(display);

		for(i = 0; i < img_size; i++)
			*(unsigned long *)(((unsigned long)xdata) + i * pixel_size) = color[0];

		memset(keyboard, 0, sizeof(keyboard));
		memset(keymap, 0, sizeof(keymap));

		keymap[XKC_ESC]  = 23; /* Esc */
		keymap[XKC_AE01] = 6;  /* 1 */
		keymap[XKC_AE02] = 5;  /* 2 */
		keymap[XKC_AE03] = 4;  /* 3 */
		keymap[XKC_AE04] = 3;  /* 4 */
		keymap[XKC_AE05] = 2;  /* 5 */
		keymap[XKC_AE06] = 1;  /* 6 */
		keymap[XKC_AE07] = 14; /* 7 */
		keymap[XKC_AE08] = 13; /* 8 */
		keymap[XKC_AE09] = 12; /* 9 */
		keymap[XKC_AE10] = 11; /* 0 */
		keymap[XKC_BKSP] = 10; /* Del */

		keymap[XKC_AD01] = 22; /* q */
		keymap[XKC_AD02] = 21; /* w */
		keymap[XKC_AD03] = 20; /* e */
		keymap[XKC_AD04] = 19; /* r */
		keymap[XKC_AD05] = 18; /* t */
		keymap[XKC_AD06] = 17; /* y */
		keymap[XKC_AD07] = 30; /* u */
		keymap[XKC_AD08] = 29; /* i */
		keymap[XKC_AD09] = 28; /* o */
		keymap[XKC_AD10] = 27; /* p */
		keymap[XKC_RTRN] = 25; /* Enter */

		keymap[XKC_TAB]  = 38; /* Tab */
		keymap[XKC_AC01] = 37; /* a */
		keymap[XKC_AC02] = 36; /* s */
		keymap[XKC_AC03] = 35; /* d */
		keymap[XKC_AC04] = 34; /* f */
		keymap[XKC_AC05] = 33; /* g */
		keymap[XKC_AC06] = 46; /* h */
		keymap[XKC_AC07] = 45; /* j */
		keymap[XKC_AC08] = 44; /* k */
		keymap[XKC_AC09] = 26; /* l */
		keymap[XKC_AC10] = 9;  /* : */

		keymap[XKC_LFSH] = 55; /* Left Shift */
		keymap[XKC_AB01] = 54; /* z */
		keymap[XKC_AB02] = 53; /* x */
		keymap[XKC_AB03] = 52; /* c */
		keymap[XKC_AB04] = 51; /* v */
		keymap[XKC_AB05] = 50; /* b */
		keymap[XKC_AB06] = 49; /* n */
		keymap[XKC_AB07] = 43; /* m */
		keymap[XKC_AC11] = 42; /* ' */
		keymap[XKC_UP]   = 60; /* Up */
		keymap[XKC_RTSH] = 63; /* Right Shift */

		keymap[XKC_LCTL] = 39; /* Ctrl */
		keymap[XKC_LWIN] = 47; /* Fn */
		keymap[XKC_LALT] = 31; /* Menu */
		keymap[XKC_SPCE] = 61; /* space */
		keymap[XKC_AB10] = 59; /* ? */
		keymap[XKC_LEFT] = 58; /* Left */
		keymap[XKC_DOWN] = 41; /* Down */
		keymap[XKC_RGHT] = 47; /* Right */
	}
	XPutImage(display, win, gc, ximage, 0, 0, 0, 0, width, height);
	lcd_enabled = 1;
}

void
lcd_disable(ARMul_State *state)
{
	if(win)
	{
		XSetForeground(display, gc, cr2pv(0x00808080));
		XFillRectangle(display, win, gc, 0, 0, lcd_width, lcd_height);
	}
	lcd_enabled = 0;
}

void
lcd_write(ARMul_State *state, ARMword addr, ARMword data)
{
	ARMword offset, pal;
	int pixnum, x, y, bit;

	if(!ximage) return;
	
	offset = (addr & ~3) - LCD_BASE;
	pixnum = offset * 8 / lcd_depth;
	x = pixnum % lcd_width;
	y = pixnum / lcd_width;
	
	for (bit = 0; bit < 32; bit += lcd_depth, pixnum++) {
		int pix = (data >> bit) % (1 << lcd_depth);
		pal = (pix & 8) ? state->io.palmsw : state->io.pallsw;
		*(unsigned short *)(((unsigned long)xdata) + pixnum * 2) = color[(pal >> (pix * 4)) & 15];
	}
	XPutImage(display, win, gc, ximage, x, y, x, y, 32 / lcd_depth, 1);
}
