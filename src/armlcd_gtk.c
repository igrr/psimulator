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
#include <gtk/gtk.h>

#include "armdefs.h"

#define MAX_DEPTH	4		/* bits per pixel */
#define GREY_LEVELS	16
#define LCD_BASE	0xC0000000

static GtkWidget *window;
static GtkWidget *screen;
static GdkGC *gc[GREY_LEVELS];

static int lcd_width, lcd_height, lcd_depth;

extern ARMul_State *state;
extern unsigned char keyboard[8];

int keymap[256];

static gint
expose_event (GtkWidget *widget, GdkEventExpose *event)
{int x, y, pix, bit, line_len;
 ARMword *plcd = state->mem.dram, pal, *data;
	/* event->area.x, event->area.y, event->area.width, event->area.height */
	if(!plcd) return FALSE;
	if(!GTK_WIDGET_DRAWABLE(widget)) return FALSE;
	line_len = lcd_width * lcd_depth / 32;
	plcd += event->area.y * line_len;
	for(y = event->area.y; y < event->area.height; y++) {
		data = plcd + event->area.x * lcd_depth / 32;
		for(x = event->area.x; x < event->area.width;) {
			for (bit = 0; bit < 32; bit += lcd_depth) {
				pix = (*data >> bit) % (1 << lcd_depth);
				pal = (pix & 8) ? state->io.palmsw : state->io.pallsw;
				gdk_draw_point(widget->window, gc[(pal >> (pix * 4)) & 15], x, y);
				x++;
			}
			data++;
		}
		plcd += line_len;
	}
	return FALSE;
}

static gint
button_press_event (GtkWidget *widget, GdkEventButton *event)
{
	/* event->x, event->y, event->button */
	printf("Screen press: %d %d\n", event->x, event->y);
	if(event->y > lcd_height)
	{
	
	}
	return TRUE;
}

static gint
motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
	/* event->x, event->y */
	return TRUE;
}

static gint
key_press_event (GtkWidget *widget, GdkEventKey *event)
{int code;
	/* event->keyval event->string */
	printf("Key press: %#04x %s\n", event->keyval, event->string);
	code = keymap[event->keyval & 0xFF];
	if(code--)
		keyboard[code >> 3] |= (1 << (code & 7));
	return TRUE;
}

static gint
key_release_event (GtkWidget *widget, GdkEventKey *event)
{int code;
	/* event->keyval event->string */
	printf("Key release: %#04x %s\n", event->keyval, event->string);
	code = keymap[event->keyval & 0xFF];
	if(code--)
		keyboard[code >> 3] &= ~(1 << (code & 7));
	return TRUE;
}

extern int global_argc;
extern char **global_argv;


void
lcd_cycle(ARMul_State *state)
{
	gtk_main_iteration_do(FALSE);
}

void
lcd_enable(ARMul_State *state, int width, int height, int depth)
{
	int i;
	static int once = 0;
	GdkColormap *colormap;

	if (!once) {
		once++;
		gtk_init(&global_argc, &global_argv);
	}
	lcd_width = width;
	lcd_height = height;
	lcd_depth = depth;
	state->io.lcd_limit = LCD_BASE + (width * height * depth / 8);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_name(window, "ARMulator");
	gtk_window_set_title(GTK_WINDOW(window), "ARMulator - Psion Series 5");

	screen = gtk_drawing_area_new ();
	gtk_drawing_area_size (GTK_DRAWING_AREA (screen), width, height * 2);
	gtk_container_add (GTK_CONTAINER (window), screen);

	gtk_signal_connect(GTK_OBJECT(screen), "expose_event",
					(GtkSignalFunc)expose_event, NULL);
	gtk_signal_connect(GTK_OBJECT(screen), "motion_notify_event",
					(GtkSignalFunc)motion_notify_event, NULL);
	gtk_signal_connect(GTK_OBJECT(screen), "button_press_event",
					(GtkSignalFunc)button_press_event, NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
					(GtkSignalFunc)key_press_event, NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_release_event",
					(GtkSignalFunc)key_release_event, NULL);


	gtk_widget_set_events(screen, GDK_EXPOSURE_MASK
					 | GDK_LEAVE_NOTIFY_MASK
					 | GDK_BUTTON_PRESS_MASK
					 | GDK_POINTER_MOTION_MASK
					 | GDK_POINTER_MOTION_HINT_MASK);

	gtk_widget_set_events(window, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	gtk_window_set_policy(GTK_WINDOW(window), 0, 0, 0);
	gtk_widget_show_all(window);

	colormap = gdk_window_get_colormap(screen->window);
	for (i = 0; i < GREY_LEVELS; i++) {
			GdkColor color;

		/* #7fc5a7 is a nice LCDish colour */
		color.red = (15 - i) * (0x7f00 / GREY_LEVELS);
		color.green = (15 - i) * (0xc500 / GREY_LEVELS);
		color.blue = (15 - i) * (0xa700 / GREY_LEVELS);
		gdk_color_alloc (colormap, &color);
		gc[i] = gdk_gc_new(screen->window);
		gdk_gc_set_foreground(gc[i], &color);
	}
	memset(keyboard, 0, sizeof(keyboard));
	memset(keymap, 0, sizeof(keymap));
	keymap[0x0d] = 25; /* Enter */
	keymap['a'] = 37;
	keymap['b'] = 50;
	keymap['c'] = 52;
	keymap['d'] = 35;
	keymap['e'] = 20;
	keymap['f'] = 34;
	keymap['g'] = 33;
	keymap['h'] = 46;
	keymap['i'] = 29;
	keymap['j'] = 45;
	keymap['k'] = 44;
	keymap['l'] = 26;
	keymap['m'] = 43;
	keymap['n'] = 49;
	keymap['o'] = 28;
	keymap['p'] = 27;
	keymap['q'] = 22;
	keymap['r'] = 19;
	keymap['s'] = 36;
	keymap['t'] = 18;
	keymap['u'] = 30;
	keymap['v'] = 51;
	keymap['w'] = 21;
	keymap['x'] = 53;
	keymap['y'] = 17;
	keymap['z'] = 54;
	keymap['0'] = 11;
	keymap['1'] = 6;
	keymap['2'] = 5;
	keymap['3'] = 4;
	keymap['4'] = 3;
	keymap['5'] = 2;
	keymap['6'] = 1;
	keymap['7'] = 14;
	keymap['8'] = 13;
	keymap['9'] = 12;
	keymap[' '] = 61;
//	gdk_key_repeat_disable();
}

void
lcd_disable(ARMul_State *state)
{
	int i;
//	gdk_key_repeat_restore();
	for (i = 0; i < GREY_LEVELS; i++) {
		if (gc[i]) {
			gdk_gc_destroy(gc[i]);
			gc[i] = NULL;
		}
	}
	if (window) {
		gtk_widget_destroy(window);
		window = NULL;
	}
}

void
lcd_write(ARMul_State *state, ARMword addr, ARMword data)
{
	ARMword offset, pal;
	int pixnum, x, y, bit;
	
	offset = (addr & ~3) - LCD_BASE;
	pixnum = offset * 8 / lcd_depth;
	x = pixnum % lcd_width;
	y = pixnum / lcd_width;
	assert(y < lcd_height);
	
	for (bit = 0; bit < 32; bit += lcd_depth) {
		int pix = (data >> bit) % (1 << lcd_depth);
		pal = (pix & 8) ? state->io.palmsw : state->io.pallsw;
		gdk_draw_point(screen->window, gc[(pal >> (pix * 4)) & 15], x, y);
		x++;
	}
}
