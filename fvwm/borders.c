/* -*-c-*- */
/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/****************************************************************************
 * This module is all original code
 * by Rob Nation
 * Copyright 1993, Robert Nation
 *     You may use this code for any purpose, as long as the original
 *     copyright remains in the source code and all documentation
 ****************************************************************************/

/* ---------------------------- included header files ----------------------- */

#include "config.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>

#include "libs/fvwmlib.h"
#include "libs/FShape.h"
#include "libs/Flocale.h"
#include <libs/gravity.h>
#include "fvwm.h"
#include "externs.h"
#include "events.h"
#include "cursor.h"
#include "functions.h"
#include "bindings.h"
#include "misc.h"
#include "screen.h"
#include "defaults.h"
#include "geometry.h"
#include "borders.h"
#include "builtins.h"
#include "icons.h"
#include "module_interface.h"
#include "libs/Colorset.h"
#include "add_window.h"
#include "frame.h"

/* ---------------------------- local definitions --------------------------- */

/* ---------------------------- local macros -------------------------------- */

/* ---------------------------- imports ------------------------------------- */

extern Window PressedW;

/* ---------------------------- included code files ------------------------- */

/* ---------------------------- local types --------------------------------- */

typedef struct
{
	struct
	{
		unsigned use_pixmap : 1;
	} flags;
	Pixel pixel;
	struct
	{
		Pixmap p;
		Pixmap shape;
		size_rect size;
		struct
		{
			unsigned is_tiled : 1;
		} flags;
	} pixmap;
} pixmap_background_type;

typedef struct
{
	int relief_width;
	GC relief_gc;
	GC shadow_gc;
	Pixel fore_color;
	Pixel back_color;
	Pixmap back_pixmap;
	XSetWindowAttributes attributes;
	unsigned long valuemask;
	Pixmap texture_pixmap;
	XSetWindowAttributes notex_attributes;
	unsigned long notex_valuemask;
	struct
	{
		unsigned has_color_changed : 1;
	} flags;
} common_decorations_type;

typedef struct
{
	GC relief;
	GC shadow;
	GC transparent;
} draw_border_gcs;

typedef struct
{
	int offset_tl;
	int offset_br;
	int thickness;
	int length;
	unsigned has_x_marks : 1;
	unsigned has_y_marks : 1;
} border_marks_descr;

typedef struct
{
	int w_dout;
	int w_hiout;
	int w_trout;
	int w_c;
	int w_trin;
	int w_shin;
	int w_din;
	int sum;
	int trim;
	unsigned is_flat : 1;
} border_relief_size_descr;

typedef struct
{
	rectangle sidebar_g;
	border_relief_size_descr relief;
	border_marks_descr marks;
	draw_border_gcs gcs;
} border_relief_descr;

typedef struct
{
	unsigned pressed_bmask : NUMBER_OF_BUTTONS;
	unsigned lit_bmask : NUMBER_OF_BUTTONS;
	unsigned toggled_bmask : NUMBER_OF_BUTTONS;
	unsigned clear_bmask : NUMBER_OF_BUTTONS;
	unsigned draw_bmask : NUMBER_OF_BUTTONS;
	ButtonState bstate[NUMBER_OF_BUTTONS];
	unsigned is_title_pressed : 1;
	unsigned is_title_lit : 1;
	unsigned is_title_toggled : 1;
	unsigned do_clear_title : 1;
	ButtonState tstate;
} border_titlebar_state;

typedef struct
{
	common_decorations_type *cd;
	rectangle frame_g;
	frame_title_layout_type layout;
	frame_title_layout_type old_layout;
	border_titlebar_state tbstate;
} titlebar_descr;

/* ---------------------------- forward declarations ------------------------ */

/* ---------------------------- local variables ----------------------------- */

static const char ulgc[] = { 1, 0, 0, 0x7f, 2, 1, 1 };
static const char brgc[] = { 1, 1, 2, 0x7f, 0, 0, 3 };

/* ---------------------------- exported variables (globals) ---------------- */

XGCValues Globalgcv;
unsigned long Globalgcm;

/* ---------------------------- local functions ----------------------------- */

/*!!!remove*/
static void print_g(char *text, rectangle *g)
{
	if (g == NULL)
	{
		fprintf(stderr, "%s: (null)", (text == NULL) ? "" : text);
	}
	else
	{
		fprintf(stderr, "%s: %4d %4d %4dx%4d (%4d - %4d %4d - %4d)\n",
			(text == NULL) ? "" : text,
			g->x, g->y, g->width, g->height,
			g->x, g->x + g->width - 1, g->y, g->y + g->height - 1);
	}
}

/****************************************************************************
 *
 *  Redraws buttons (veliaa@rpi.edu)
 *
 ****************************************************************************/
void DrawButton(
	FvwmWindow *t, Window win, int w, int h, DecorFace *df, GC ReliefGC,
	GC ShadowGC, Pixel fore_color, Pixel back_color,
	mwm_flags stateflags, int is_toggled, int left1right0)
{
	register DecorFaceType type = DFS_FACE_TYPE(df->style);
	Picture *p;
	int border = 0;
	int x;
	int y;
	int width;
	int height;

	switch (type)
	{
	case SimpleButton:
		break;

	case SolidButton:
		/*!!!*/
		break;

	case VectorButton:
	case DefaultVectorButton:
#if 0
		if (is_toggled)
		{
			DrawLinePattern(
				win, ShadowGC, ReliefGC, fore_color, back_color,
				&df->u.vector, w, h);
		}
		else
		{
			DrawLinePattern(
				win, ReliefGC, ShadowGC, fore_color, back_color,
				&df->u.vector, w, h);
		}
#endif
		break;

	case MiniIconButton:
	case PixmapButton:
	case TiledPixmapButton:
#ifdef FANCY_TITLEBARS
	case MultiPixmap:       /* in case of UseTitleStyle */
#endif
#ifdef FANCY_TITLEBARS
		if (type == MultiPixmap)
		{
			if (left1right0 && df->u.multi_pixmaps[TBP_LEFT_BUTTONS])
				p = df->u.multi_pixmaps[TBP_LEFT_BUTTONS];
			else if (!left1right0 && df->u.multi_pixmaps[TBP_RIGHT_BUTTONS])
				p = df->u.multi_pixmaps[TBP_RIGHT_BUTTONS];
			else if (df->u.multi_pixmaps[TBP_BUTTONS])
				p = df->u.multi_pixmaps[TBP_BUTTONS];
			else if (left1right0 && df->u.multi_pixmaps[TBP_LEFT_MAIN])
				p = df->u.multi_pixmaps[TBP_LEFT_MAIN];
			else if (!left1right0 && df->u.multi_pixmaps[TBP_RIGHT_MAIN])
				p = df->u.multi_pixmaps[TBP_RIGHT_MAIN];
			else
				p = df->u.multi_pixmaps[TBP_MAIN];
		}
#endif
		else if (FMiniIconsSupported && type == MiniIconButton)
		{
			if (!t->mini_icon)
				break;
			p = t->mini_icon;
		}
		else
		{
			p = df->u.p;
		}

		if (DFS_BUTTON_RELIEF(df->style) == DFS_BUTTON_IS_FLAT)
			border = 0;
		else
			border = HAS_MWM_BORDER(t) ? 1 : 2;
		x = border;
		y = border;
		width = w - border * 2;
		height = h - border * 2;

		if (type != TiledPixmapButton
#ifdef FANCY_TITLEBARS
		    && type != MultiPixmap
#endif
			)
		{
			if (DFS_H_JUSTIFICATION(df->style) == JUST_RIGHT)
				x += (int)(width - p->width);
			else if (DFS_H_JUSTIFICATION(df->style) == JUST_CENTER)
				/* round up for left buttons, down for right buttons */
				x += (int)(width - p->width + left1right0) / 2;
			if (DFS_V_JUSTIFICATION(df->style) == JUST_BOTTOM)
				y += (int)(height - p->height);
			else if (DFS_V_JUSTIFICATION(df->style) == JUST_CENTER)
				/* round up */
				y += (int)(height - p->height + 1) / 2;
			if (x < border)
				x = border;
			if (y < border)
				y = border;
			if (width > p->width)
				width = p->width;
			if (height > p->height)
				height = p->height;
			if (width > w - x - border)
				width = w - x - border;
			if (height > h - y - border)
				height = h - y - border;
		}

		XSetClipMask(dpy, Scr.TransMaskGC, p->mask);
		XSetClipOrigin(dpy, Scr.TransMaskGC, x, y);
		if (type != TiledPixmapButton
#ifdef FANCY_TITLEBARS
		    && type != MultiPixmap
#endif
			)
		{
			int cx = x;
			int cy = y;
			int cw = width;
			int ch = height;
			int ox = 0;
			int oy = 0;

			if (cw > 0 && ch > 0)
			{
				XCopyArea(
					dpy, p->picture, win, Scr.TransMaskGC,
					ox, oy, cw, ch, cx, cy);
			}
		}
		else
		{
			int xi;
			int yi;

			for (yi = border; yi < height; yi += p->height)
			{
				for (xi = border; xi < width; xi += p->width)
				{
					int lw = width - xi;
					int lh = height - yi;
					int cx;
					int cy;
					int cw;
					int ch;
					int ox;
					int oy;

					if (lw > p->width)
						lw = p->width;
					if (lh > p->height)
						lh = p->height;
					cx = xi;
					cy = yi;
					cw = lw;
					ch = lh;
					ox = 0;
					oy = 0;
					if (cw > 0 && ch > 0)
					{
						XSetClipOrigin(dpy, Scr.TransMaskGC, xi, yi);
						XCopyArea(dpy, p->picture, win, Scr.TransMaskGC,
							  ox, oy, cw, ch, cx, cy);
					}
				}
			}
		}
		XSetClipMask(dpy, Scr.TransMaskGC, None);
		break;

	case GradientButton:
	{
		unsigned int g_width;
		unsigned int g_height;

		/* find out the size the pixmap should be */
		CalculateGradientDimensions(
			dpy, win, df->u.grad.npixels, df->u.grad.gradient_type,
			&g_width, &g_height);
		/* draw the gradient directly into the window */
		CreateGradientPixmap(
			dpy, win, Scr.TransMaskGC, df->u.grad.gradient_type,
			g_width, g_height, df->u.grad.npixels,
			df->u.grad.pixels, win, 0, 0, w, h, NULL);
	}
	break;

	default:
		fvwm_msg(ERR, "DrawButton", "unknown button type");
		break;
	}

	return;
}

#ifdef FANCY_TITLEBARS

/****************************************************************************
 *
 *  Tile or stretch src into dest, starting at the given location and
 *  continuing for the given width and height. This is a utility function used
 *  by DrawMultiPixmapTitlebar. (tril@igs.net)
 *
 ****************************************************************************/
static void RenderIntoWindow(
	GC gc, Picture *src, Window dest, int x_start, int y_start, int width,
	int height, Bool stretch)
{
	Pixmap pm;

	if (stretch)
	{
		pm = CreateStretchPixmap(
			dpy, src->picture, src->width, src->height, src->depth,
			width, height, gc);
	}
	else
	{
		pm = CreateTiledPixmap(
			dpy, src->picture, src->width, src->height, width,
			height, src->depth, gc);
	}
	XCopyArea(dpy, pm, dest, gc, 0, 0, width, height, x_start, y_start);
	XFreePixmap(dpy, pm);

	return;
}

static int get_multipm_length(
	FvwmWindow *fw, Picture **pm, int part)
{
	if (pm[part] == NULL)
	{
		return 0;
	}
	else if (HAS_VERTICAL_TITLE(fw))
	{
		return pm[part]->height;
	}
	else
	{
		return pm[part]->width;
	}
}

/****************************************************************************
 *
 *  Redraws multi-pixmap titlebar (tril@igs.net)
 *
 ****************************************************************************/
#define SWAP_ARGS(f,a1,a2) (f)?(a2):(a1),(f)?(a1):(a2)
static void DrawMultiPixmapTitlebar(FvwmWindow *fw, DecorFace *df)
{
	Window title_win;
	GC gc;
	char *title;
	Picture **pm;
	short stretch_flags;
	int text_length, text_pos, text_offset;
	int before_space, after_space, under_offset, under_width;
	int size;
	FlocaleWinString fstr;
	rectangle tmp_g;
	Bool has_vt = HAS_VERTICAL_TITLE(fw);

	pm = df->u.multi_pixmaps;
	stretch_flags = df->u.multi_stretch_flags;
	gc = Scr.TitleGC;
	XSetClipMask(dpy, gc, None);
	title_win = FW_W_TITLE(fw);
	title = fw->visible_name;

	tmp_g.width = 0;
	tmp_g.height = 0;
	get_title_geometry(fw, &tmp_g);
	if (pm[TBP_MAIN])
	{
		RenderIntoWindow(
			gc, pm[TBP_MAIN], title_win, 0, 0, tmp_g.width,
			tmp_g.height, (stretch_flags & (1 << TBP_MAIN)));
	}
	else if (!title)
	{
		RenderIntoWindow(
			gc, pm[TBP_LEFT_MAIN], title_win, 0, 0, tmp_g.width,
			tmp_g.height, (stretch_flags & (1 << TBP_LEFT_MAIN)));
	}

	if (title)
	{
		int len = strlen(title);

		if (has_vt)
		{
			len = -len;
		}
		text_length = FlocaleTextWidth(fw->title_font, title, len);
		if (text_length > fw->title_length)
		{
			text_length = fw->title_length;
		}
		switch (TB_JUSTIFICATION(GetDecor(fw, titlebar)))
		{
		case JUST_LEFT:
			text_pos = TITLE_PADDING;
			text_pos += get_multipm_length(
				fw, pm, TBP_LEFT_OF_TEXT);
			text_pos += get_multipm_length(
				fw, pm, TBP_LEFT_END);
			if (text_pos > fw->title_length - text_length)
			{
				text_pos = fw->title_length - text_length;
			}
			break;
		case JUST_RIGHT:
			text_pos = fw->title_length - text_length -
				TITLE_PADDING;
			text_pos -= get_multipm_length(
				fw, pm, TBP_RIGHT_OF_TEXT);
			text_pos -= get_multipm_length(
				fw, pm, TBP_RIGHT_END);
			if (text_pos < 0)
			{
				text_pos = 0;
			}
			break;
		default:
			text_pos = (fw->title_length - text_length) / 2;
			break;
		}

		under_offset = text_pos;
		under_width = text_length;
		/* If there's title padding, put it *inside* the undertext
		 * area: */
		if (under_offset >= TITLE_PADDING)
		{
			under_offset -= TITLE_PADDING;
			under_width += TITLE_PADDING;
		}
		if (under_offset + under_width + TITLE_PADDING <=
		    fw->title_length)
		{
			under_width += TITLE_PADDING;
		}
		before_space = under_offset;
		after_space = fw->title_length - before_space -
			under_width;

		if (pm[TBP_LEFT_MAIN] && before_space > 0)
		{
			RenderIntoWindow(
				gc, pm[TBP_LEFT_MAIN], title_win, 0, 0,
				SWAP_ARGS(has_vt, before_space,
					  fw->title_thickness),
				(stretch_flags & (1 << TBP_LEFT_MAIN)));
		}
		if (pm[TBP_RIGHT_MAIN] && after_space > 0)
		{
			RenderIntoWindow(
				gc, pm[TBP_RIGHT_MAIN], title_win,
				SWAP_ARGS(has_vt, under_offset + under_width,
					  0),
				SWAP_ARGS(has_vt, after_space,
					  fw->title_thickness),
				(stretch_flags & (1 << TBP_RIGHT_MAIN)));
		}
		if (pm[TBP_UNDER_TEXT] && under_width > 0)
		{
			RenderIntoWindow(
				gc, pm[TBP_UNDER_TEXT], title_win,
				SWAP_ARGS(has_vt, under_offset, 0),
				SWAP_ARGS(has_vt, under_width,
					  fw->title_thickness),
				(stretch_flags & (1 << TBP_UNDER_TEXT)));
		}
		size = get_multipm_length(fw, pm, TBP_LEFT_OF_TEXT);
		if (size > 0 && size <= before_space)
		{

			XCopyArea(
				dpy, pm[TBP_LEFT_OF_TEXT]->picture, title_win,
				gc, 0, 0,
				SWAP_ARGS(has_vt, size,
					  fw->title_thickness),
				SWAP_ARGS(has_vt, under_offset - size, 0));
			before_space -= size;
		}
		size = get_multipm_length(fw, pm, TBP_RIGHT_OF_TEXT);
		if (size > 0 && size <= after_space)
		{
			XCopyArea(
				dpy, pm[TBP_RIGHT_OF_TEXT]->picture, title_win,
				gc, 0, 0,
				SWAP_ARGS(has_vt, size,
					  fw->title_thickness),
				SWAP_ARGS(has_vt, under_offset + under_width,
					  0));
			after_space -= size;
		}
		size = fw->title_thickness;
		text_offset = fw->title_text_offset;
		memset(&fstr, 0, sizeof(fstr));
		fstr.str = fw->visible_name;
		fstr.win = title_win;
		if (has_vt)
		{
			fstr.x = text_offset;
			fstr.y = text_pos;
		}
		else
		{
			fstr.y = text_offset;
			fstr.x = text_pos;
		}
		fstr.gc = gc;
		fstr.flags.text_rotation = fw->title_text_rotation;
		FlocaleDrawString(dpy, fw->title_font, &fstr, 0);
	}
	else
	{
		before_space = fw->title_length / 2;
		after_space = fw->title_length / 2;
	}

	size = get_multipm_length(fw, pm, TBP_LEFT_END);
	if (size > 0 && size <= before_space)
	{
		XCopyArea(
			dpy, pm[TBP_LEFT_END]->picture, title_win, gc, 0, 0,
			SWAP_ARGS(has_vt, size, fw->title_thickness),
			0, 0);
	}
	size = get_multipm_length(fw, pm, TBP_RIGHT_END);
	if (size > 0 && size <= after_space)
	{
		XCopyArea(
			dpy, pm[TBP_RIGHT_END]->picture, title_win, gc, 0, 0,
			SWAP_ARGS(has_vt, size, fw->title_thickness),
			fw->title_length - size, 0);
	}

	return;
}

#endif /* FANCY_TITLEBARS */

static Bool is_title_toggled(
	FvwmWindow *fw)
{
	if (!HAS_MWM_BUTTONS(fw))
	{
		return False;
	}
	if (TB_HAS_MWM_DECOR_MAXIMIZE(GetDecor(fw, titlebar)) &&
	    IS_MAXIMIZED(fw))
	{
		return True;
	}
	if (TB_HAS_MWM_DECOR_SHADE(GetDecor(fw, titlebar)) && IS_SHADED(fw))
	{
		return True;
	}
	if (TB_HAS_MWM_DECOR_STICK(GetDecor(fw, titlebar)) && IS_STICKY(fw))
	{
		return True;
	}

	return False;
}

static Bool is_button_toggled(
	FvwmWindow *fw, int button)
{
	mwm_flags mf;

	if (!HAS_MWM_BUTTONS(fw))
	{
		return False;
	}
	mf = TB_MWM_DECOR_FLAGS(GetDecor(fw, buttons[button]));
	if ((mf & MWM_DECOR_MAXIMIZE) && IS_MAXIMIZED(fw))
	{
		return True;
	}
	if ((mf & MWM_DECOR_SHADE) && IS_SHADED(fw))
	{
		return True;
	}
	if ((mf & MWM_DECOR_STICK) && IS_STICKY(fw))
	{
		return True;
	}
	if (TB_FLAGS(fw->decor->buttons[button]).has_layer &&
	    fw->layer == TB_LAYER(fw->decor->buttons[button]))
	{
		return True;
	}

	return False;
}


/* rules to get button state */
static ButtonState border_flags_to_button_state(
	int is_pressed, int is_lit, int is_toggled)
{
	if (Scr.gs.use_active_down_buttons)
	{
		if (Scr.gs.use_inactive_buttons && !is_lit)
		{
			return (is_toggled) ? BS_ToggledInactive : BS_Inactive;
		}
		else
		{
			if (is_pressed)
			{
				return (is_toggled) ?
					BS_ToggledActiveDown : BS_ActiveDown;
			}
			else
			{
				return (is_toggled) ?
					BS_ToggledActiveUp : BS_ActiveUp;
			}
		}
	}
	else
	{
		if (Scr.gs.use_inactive_buttons && !is_lit)
		{
			return (is_toggled) ? BS_ToggledInactive : BS_Inactive;
		}
		else
		{
			return (is_toggled) ? BS_ToggledActiveUp : BS_ActiveUp;
		}
	}
}

/****************************************************************************
 *
 *  Redraws just the title bar
 *
 ****************************************************************************/
void RedrawTitle(
	common_decorations_type *cd, FvwmWindow *t, Bool has_focus)
{
  int offset;
  int length;
  int i;
  FlocaleWinString fstr;
  ButtonState title_state;
  DecorFaceStyle *tb_style;
  Pixmap *pass_bg_pixmap;
  GC rgc = cd->relief_gc;
  GC sgc = cd->shadow_gc;
  Bool reverse = False;
  Bool is_clipped = False;
  Bool has_vt;
  Bool toggled = is_title_toggled(t);

  if (PressedW == FW_W_TITLE(t))
  {
    GC tgc;

    tgc = sgc;
    sgc = rgc;
    rgc = tgc;
  }
#if 0
  flush_expose(FW_W_TITLE(t));
#endif

  if (t->visible_name != (char *)NULL)
  {
    length = FlocaleTextWidth(
      t->title_font, t->visible_name,
      (HAS_VERTICAL_TITLE(t)) ?
      -strlen(t->visible_name) : strlen(t->visible_name));
    if (length > t->title_length - 12)
      length = t->title_length - 4;
    if (length < 0)
      length = 0;
  }
  else
  {
    length = 0;
  }

  title_state = border_flags_to_button_state(
	  (FW_W_TITLE(t) == PressedW), has_focus, toggled);
  tb_style = &TB_STATE(GetDecor(t, titlebar))[title_state].style;
  switch (TB_JUSTIFICATION(GetDecor(t, titlebar)))
  {
  case JUST_LEFT:
    offset = WINDOW_TITLE_TEXT_OFFSET;
    break;
  case JUST_RIGHT:
    offset = t->title_length - length - WINDOW_TITLE_TEXT_OFFSET;
    break;
  case JUST_CENTER:
  default:
    offset = (t->title_length - length) / 2;
    break;
  }

  NewFontAndColor(t->title_font, cd->fore_color, cd->back_color);

  /* the next bit tries to minimize redraw (veliaa@rpi.edu) */
  /* we need to check for UseBorderStyle for the titlebar */
  {
    DecorFace *df = (has_focus) ?
      &GetDecor(t,BorderStyle.active) : &GetDecor(t,BorderStyle.inactive);

    if (DFS_USE_BORDER_STYLE(*tb_style) &&
	DFS_FACE_TYPE(df->style) == TiledPixmapButton &&
	df->u.p)
    {
      XSetWindowBackgroundPixmap(dpy, FW_W_TITLE(t), df->u.p->picture);
      t->title_background_pixmap = df->u.p->picture;
      pass_bg_pixmap = NULL;
    }
    else
    {
      pass_bg_pixmap = &t->title_background_pixmap;
    }
  }

  /*
   * draw title
   */
  memset(&fstr, 0, sizeof(fstr));
  fstr.str = t->visible_name;
  fstr.win = FW_W_TITLE(t);
  fstr.flags.text_rotation = t->title_text_rotation;
  if (HAS_VERTICAL_TITLE(t))
  {
    fstr.y = offset;
    fstr.x = t->title_text_offset + 1;
    has_vt = True;
  }
  else
  {
    fstr.x = offset;
    fstr.y = t->title_text_offset + 1;
    has_vt = False;
  }
  fstr.gc = Scr.TitleGC;
  if (Pdepth < 2)
  {
    XFillRectangle(
      dpy, FW_W_TITLE(t), ((PressedW == FW_W_TITLE(t)) ? sgc : rgc),
      offset - 2, 0, length+4, t->title_thickness);
    if(t->visible_name != (char *)NULL)
    {
      FlocaleDrawString(dpy, t->title_font, &fstr, 0);
    }
    /* for mono, we clear an area in the title bar where the window
     * title goes, so that its more legible. For color, no need */
    RelieveRectangle(
      dpy, FW_W_TITLE(t), 0, 0,
      SWAP_ARGS(has_vt, offset - 3, t->title_thickness - 1),
      rgc, sgc, cd->relief_width);
    RelieveRectangle(
      dpy, FW_W_TITLE(t),
      SWAP_ARGS(has_vt, offset + length + 2, 0),
      SWAP_ARGS(has_vt, t->title_length - length - offset - 3,
		t->title_thickness - 1),
      rgc, sgc, cd->relief_width);
    XDrawLine(
      dpy, FW_W_TITLE(t), sgc,
      SWAP_ARGS(has_vt, 0, offset + length + 1),
      SWAP_ARGS(has_vt, offset + length + 1, t->title_thickness));
  }
  else
  {
    DecorFace *df = &TB_STATE(GetDecor(t, titlebar))[title_state];
    /* draw compound titlebar (veliaa@rpi.edu) */

#ifdef FANCY_TITLEBARS
    if (df->style.face_type == MultiPixmap)
      DrawMultiPixmapTitlebar(t, df);
    else
#endif
    if (PressedW == FW_W_TITLE(t))
    {
      for (; df; df = df->next)
      {
	DrawButton(
	  t, FW_W_TITLE(t),
	  SWAP_ARGS(has_vt, t->title_length, t->title_thickness),
	  df, sgc, rgc, cd->fore_color, cd->back_color, 0, toggled, 1);
      }
    }
    else
    {
      for (; df; df = df->next)
      {
	DrawButton(
	  t, FW_W_TITLE(t),
	  SWAP_ARGS(has_vt, t->title_length, t->title_thickness),
	  df, rgc, sgc, cd->fore_color, cd->back_color, 0, toggled, 1);
      }
    }

#ifdef FANCY_TITLEBARS
    if (TB_STATE(GetDecor(t, titlebar))[title_state].style.face_type
        != MultiPixmap)
#endif
    {
      FlocaleDrawString(dpy, t->title_font, &fstr, 0);
    }

    /*
     * draw title relief
     */
    switch (DFS_BUTTON_RELIEF(*tb_style))
    {
    case DFS_BUTTON_IS_SUNK:
      reverse = 1;
    case DFS_BUTTON_IS_UP:
      RelieveRectangle2(
	dpy, FW_W_TITLE(t), 0, 0,
	SWAP_ARGS(has_vt, t->title_length - 1, t->title_thickness - 1),
	(reverse) ? sgc : rgc, (reverse) ? rgc : sgc, cd->relief_width);
      break;
    default:
      /* flat */
      break;
    }
  }

  /*
   * draw the 'sticky' lines
   */

  if (IS_STICKY(t) || HAS_STIPPLED_TITLE(t))
  {
    /* an odd number of lines every WINDOW_TITLE_STICK_VERT_DIST pixels */
    int num =
      (int)(t->title_thickness / WINDOW_TITLE_STICK_VERT_DIST / 2) * 2 - 1;
    int min = t->title_thickness / 2 - num * 2 + 1;
    int max = t->title_thickness / 2 + num * 2 -
	    WINDOW_TITLE_STICK_VERT_DIST + 1;
    int left_x = WINDOW_TITLE_STICK_OFFSET;
    int left_w = offset - left_x - WINDOW_TITLE_TO_STICK_GAP;
    int right_x = offset + length + WINDOW_TITLE_TO_STICK_GAP - 1;
    int right_w = t->title_length - right_x - WINDOW_TITLE_STICK_OFFSET;

    if (left_w < WINDOW_TITLE_STICK_MIN_WIDTH)
    {
      left_x = 0;
      left_w = WINDOW_TITLE_STICK_MIN_WIDTH;
    }
    if (right_w < WINDOW_TITLE_STICK_MIN_WIDTH)
    {
      right_w = WINDOW_TITLE_STICK_MIN_WIDTH;
      right_x = t->title_length - WINDOW_TITLE_STICK_MIN_WIDTH - 1;
    }
    for (i = min; i <= max; i += WINDOW_TITLE_STICK_VERT_DIST)
    {
      if (left_w > 0)
      {
	RelieveRectangle(
          dpy, FW_W_TITLE(t),
	  SWAP_ARGS(has_vt, left_x, i),
	  SWAP_ARGS(has_vt, left_w, 1),
	  sgc, rgc, 1);
      }
      if (right_w > 0)
      {
	RelieveRectangle(
          dpy, FW_W_TITLE(t),
	  SWAP_ARGS(has_vt, right_x, i),
	  SWAP_ARGS(has_vt, right_w, 1),
	  sgc, rgc, 1);
      }
    }
  }

  if (is_clipped)
  {
    XSetClipMask(dpy, rgc, None);
    XSetClipMask(dpy, sgc, None);
    XSetClipMask(dpy, Scr.TitleGC, None);
  }

  return;
}

static void get_common_decorations(
	common_decorations_type *cd, FvwmWindow *t,
	window_parts draw_parts, Bool has_focus, Bool is_border,
	Bool do_change_gcs)
{
	color_quad *draw_colors;

	if (has_focus)
	{
		/* are we using textured borders? */
		if (DFS_FACE_TYPE(
			    GetDecor(t, BorderStyle.active.style)) ==
		    TiledPixmapButton)
		{
			cd->texture_pixmap = GetDecor(
				t, BorderStyle.active.u.p->picture);
		}
		cd->back_pixmap = Scr.gray_pixmap;
		if (is_border)
		{
			draw_colors = &(t->border_hicolors);
		}
		else
		{
			draw_colors = &(t->hicolors);
		}
	}
	else
	{
		if (DFS_FACE_TYPE(GetDecor(t, BorderStyle.inactive.style)) ==
		    TiledPixmapButton)
		{
			cd->texture_pixmap = GetDecor(
				t, BorderStyle.inactive.u.p->picture);
		}
		if (IS_STICKY(t))
		{
			cd->back_pixmap = Scr.sticky_gray_pixmap;
		}
		else
		{
			cd->back_pixmap = Scr.light_gray_pixmap;
		}
		if (is_border)
		{
			draw_colors = &(t->border_colors);
		}
		else
		{
			draw_colors = &(t->colors);
		}
	}
	cd->fore_color = draw_colors->fore;
	cd->back_color = draw_colors->back;
	if (do_change_gcs)
	{
		/*!!!remove*/
		Globalgcv.foreground = draw_colors->hilight;
		Globalgcm = GCForeground;
		XChangeGC(dpy, Scr.ScratchGC1, Globalgcm, &Globalgcv);
		Globalgcv.foreground = draw_colors->shadow;
		XChangeGC(dpy, Scr.ScratchGC2, Globalgcm, &Globalgcv);
	}
	cd->relief_gc = Scr.ScratchGC1;
	cd->shadow_gc = Scr.ScratchGC2;

	/* MWMBorder style means thin 3d effects */
	cd->relief_width = (HAS_MWM_BORDER(t) ? 1 : 2);

	if (cd->texture_pixmap)
	{
		cd->attributes.background_pixmap = cd->texture_pixmap;
		cd->valuemask = CWBackPixmap;
	}
	else
	{
		if (Pdepth < 2)
		{
			cd->attributes.background_pixmap = cd->back_pixmap;
			cd->valuemask = CWBackPixmap;
		}
		else
		{
			cd->attributes.background_pixel = cd->back_color;
			cd->valuemask = CWBackPixel;
		}
	}
	if (Pdepth < 2)
	{
		cd->notex_attributes.background_pixmap = cd->back_pixmap;
		cd->notex_valuemask = CWBackPixmap;
	}
	else
	{
		cd->notex_attributes.background_pixel = cd->back_color;
		cd->notex_valuemask = CWBackPixel;
	}

	return;
}

static int border_context_to_parts(
	int context)
{
	if (context == C_FRAME || context == C_SIDEBAR ||
	    context == (C_FRAME | C_SIDEBAR))
	{
		return PART_FRAME;
	}
	else if (context == C_F_TOPLEFT)
	{
		return PART_BORDER_NW;
	}
	else if (context == C_F_TOPRIGHT)
	{
		return PART_BORDER_NE;
	}
	else if (context == C_F_BOTTOMLEFT)
	{
		return PART_BORDER_SW;
	}
	else if (context == C_F_BOTTOMRIGHT)
	{
		return PART_BORDER_SE;
	}
	else if (context == C_SB_LEFT)
	{
		return PART_BORDER_W;
	}
	else if (context == C_SB_RIGHT)
	{
		return PART_BORDER_E;
	}
	else if (context == C_SB_TOP)
	{
		return PART_BORDER_N;
	}
	else if (context == C_SB_BOTTOM)
	{
		return PART_BORDER_S;
	}
	else if (context == C_TITLE)
	{
		return PART_TITLE;
	}
	else if (context & (C_LALL | C_RALL))
	{
		return PART_BUTTONS;
	}

	return PART_NONE;
}

static int border_get_parts_and_pos_to_draw(
	common_decorations_type *cd, FvwmWindow *fw,
	window_parts pressed_parts, window_parts force_draw_parts,
	rectangle *old_g, rectangle *new_g, Bool do_hilight,
	border_relief_descr *br)
{
	window_parts draw_parts;
	window_parts parts_to_light;
	rectangle sidebar_g_old;
	DecorFaceStyle *borderstyle;
	Bool has_x_marks;
	Bool has_x_marks_old;
	Bool has_y_marks;
	Bool has_y_marks_old;

	draw_parts = 0;
	borderstyle = (do_hilight) ?
		&GetDecor(fw, BorderStyle.active.style) :
		&GetDecor(fw, BorderStyle.inactive.style);
	frame_get_sidebar_geometry(
		fw, borderstyle,  new_g, &br->sidebar_g, &has_x_marks,
		&has_y_marks);
	if (has_x_marks == True)
	{
		draw_parts |= PART_X_HANDLES;
		br->marks.has_x_marks = 1;
	}
	else
	{
		br->marks.has_x_marks = 0;
	}
	if (has_y_marks == True)
	{
		draw_parts |= PART_Y_HANDLES;
		br->marks.has_y_marks = 1;
	}
	else
	{
		br->marks.has_y_marks = 0;
	}
	draw_parts |= (pressed_parts ^ fw->decor_state.parts_inverted);
	parts_to_light = (do_hilight == True) ? PART_FRAME : PART_NONE;
	draw_parts |= (parts_to_light ^ fw->decor_state.parts_lit);
	draw_parts |= (~(fw->decor_state.parts_drawn) & PART_FRAME);
	draw_parts |= force_draw_parts;
	if (old_g == NULL)
	{
		old_g = &fw->frame_g;
	}
	if ((draw_parts & PART_FRAME) == PART_FRAME)
	{
		draw_parts |= PART_FRAME;
		return draw_parts;
	}
	frame_get_sidebar_geometry(
		fw, borderstyle, old_g, &sidebar_g_old, &has_x_marks_old,
		&has_y_marks_old);
	if (has_x_marks_old != has_x_marks)
	{
		draw_parts |= (PART_FRAME & (~(PART_BORDER_N | PART_BORDER_S)));
	}
	if (has_y_marks_old != has_y_marks)
	{
		draw_parts |= (PART_FRAME & (~(PART_BORDER_W | PART_BORDER_E)));
	}
	draw_parts |= frame_get_changed_border_parts(
		&sidebar_g_old, &br->sidebar_g);

	return draw_parts;
}

static window_parts border_get_tb_parts_to_draw(
	FvwmWindow *fw, titlebar_descr *td, window_parts force_draw_parts)
{
	window_parts draw_parts;
	ButtonState old_state;
	unsigned int max_bmask;
	int i;

	td->tbstate.draw_bmask = 0;
	draw_parts = PART_NONE;
	/* first time? */
	draw_parts |= (~(fw->decor_state.parts_drawn) & PART_TITLE);
	td->tbstate.draw_bmask |= (~(fw->decor_state.buttons_drawn));
	/* forced? */
	draw_parts |= force_draw_parts;
	td->tbstate.draw_bmask |= (force_draw_parts & PART_BUTTONS) ? ~0 : 0;
	/* check if state changed */
	old_state = border_flags_to_button_state(
		(fw->decor_state.parts_inverted & PART_TITLE),
		(fw->decor_state.parts_lit & PART_TITLE),
		(fw->decor_state.parts_toggled & PART_TITLE));
	if (old_state != td->tbstate.tstate)
	{
			draw_parts |= PART_TITLE;
	}
	/* size changed? */
	if ((td->old_layout.title_g.width != td->layout.title_g.width ||
	     td->old_layout.title_g.height != td->layout.title_g.height) &&
	    td->layout.title_g.x >= 0 && td->layout.title_g.y >= 0)
	{
		draw_parts |= PART_TITLE;
	}
	/* same for buttons */
	for (i = 0; i < NUMBER_OF_BUTTONS; i++)
	{
		unsigned int mask = (1 << i);

		if (FW_W_BUTTON(fw, i) == None)
		{
			continue;
		}
		old_state = border_flags_to_button_state(
			(fw->decor_state.buttons_inverted & mask),
			(fw->decor_state.buttons_lit & mask),
			(fw->decor_state.buttons_toggled & mask));
		if (old_state != td->tbstate.bstate[i])
		{
			draw_parts |= PART_BUTTONS;
			td->tbstate.draw_bmask |= mask;
		}
		if ((td->old_layout.button_g[i].width !=
		     td->layout.button_g[i].width ||
		     td->old_layout.button_g[i].height !=
		     td->layout.button_g[i].height) &&
		    td->layout.button_g[i].x >= 0 &&
		    td->layout.button_g[i].y >= 0)
		{
			td->tbstate.draw_bmask |= (1 << i);
		}
	}
	/*!!! position changed and background is tiled? */
	/*!!! background changed? */
	max_bmask = 0;
	for (i = 0; i < NUMBER_OF_BUTTONS; i++)
	{
		if (FW_W_BUTTON(fw, i) == None)
		{
			continue;
		}
		if ((i & 1) == 1 && i / 2 < Scr.nr_right_buttons)
		{
			max_bmask |= (1 << i);
		}
		else if ((i & 1) == 0 && i / 2 < Scr.nr_left_buttons)
		{
			max_bmask |= (1 << i);
		}
	}
	td->tbstate.draw_bmask &= max_bmask;
	td->tbstate.pressed_bmask &= max_bmask;
	td->tbstate.lit_bmask &= max_bmask;
	td->tbstate.toggled_bmask &= max_bmask;
	td->tbstate.clear_bmask &= max_bmask;
	if (td->tbstate.draw_bmask == 0)
	{
		draw_parts &= ~PART_BUTTONS;
	}

	return draw_parts;
}

static void border_get_border_gcs(
	draw_border_gcs *ret_gcs, common_decorations_type *cd, FvwmWindow *fw,
	Bool do_hilight)
{
	static GC transparent_gc = None;
	DecorFaceStyle *borderstyle;
	Bool is_reversed = False;

	if (transparent_gc == None && HAS_BORDER(fw) && !HAS_MWM_BORDER(fw))
	{
		XGCValues xgcv;

		xgcv.function = GXnoop;
		xgcv.plane_mask = 0;
		transparent_gc = fvwmlib_XCreateGC(
			dpy, FW_W_FRAME(fw), GCFunction | GCPlaneMask, &xgcv);
	}
	ret_gcs->transparent = transparent_gc;
	/* get the border style bits */
	borderstyle = (do_hilight) ?
		&GetDecor(fw, BorderStyle.active.style) :
		&GetDecor(fw, BorderStyle.inactive.style);
	if (borderstyle->flags.button_relief == DFS_BUTTON_IS_SUNK)
	{
		is_reversed = True;
	}
	if (is_reversed)
	{
		ret_gcs->shadow = cd->relief_gc;
		ret_gcs->relief = cd->shadow_gc;
	}
	else
	{
		ret_gcs->relief = cd->relief_gc;
		ret_gcs->shadow = cd->shadow_gc;
	}

	return;
}

static void trim_border_layout(
	FvwmWindow *fw, DecorFaceStyle *borderstyle,
	border_relief_size_descr *ret_size_descr)
{
	/* If the border is too thin to accomodate the standard look, we remove
	 * parts of the border so that at least one pixel of the original
	 * colour is visible. We make an exception for windows with a border
	 * width of 2, though. */
	if ((!IS_SHADED(fw) || HAS_TITLE(fw)) && fw->boundary_width == 2)
	{
		ret_size_descr->trim--;
	}
	if (ret_size_descr->trim < 0)
	{
		ret_size_descr->trim = 0;
	}
	for ( ; ret_size_descr->trim > 0; ret_size_descr->trim--)
	{
		if (ret_size_descr->w_hiout > 1)
		{
			ret_size_descr->w_hiout--;
		}
		else if (ret_size_descr->w_shin > 0)
		{
			ret_size_descr->w_shin--;
		}
		else if (ret_size_descr->w_hiout > 0)
		{
			ret_size_descr->w_hiout--;
		}
		else if (ret_size_descr->w_trout > 0)
		{
			ret_size_descr->w_trout = 0;
			ret_size_descr->w_trin = 0;
			ret_size_descr->w_din = 0;
			ret_size_descr->w_hiout = 1;
		}
		ret_size_descr->sum--;
	}
	ret_size_descr->w_c = fw->boundary_width - ret_size_descr->sum;

	return;
}

static void check_remove_inset(
	DecorFaceStyle *borderstyle, border_relief_size_descr *ret_size_descr)
{
	if (!DFS_HAS_NO_INSET(*borderstyle))
	{
		return;
	}
	ret_size_descr->w_shin = 0;
	ret_size_descr->sum--;
	ret_size_descr->trim--;
	if (ret_size_descr->w_trin)
	{
		ret_size_descr->w_trout = 0;
		ret_size_descr->w_trin = 0;
		ret_size_descr->w_din = 0;
		ret_size_descr->w_hiout = 1;
		ret_size_descr->sum -= 2;
		ret_size_descr->trim -= 2;
	}

	return;
}

static void border_fetch_mwm_layout(
	FvwmWindow *fw, DecorFaceStyle *borderstyle,
	border_relief_size_descr *ret_size_descr)
{
	/* MWM borders look like this:
	 *
	 * HHCCCCS  from outside to inside on the left and top border
	 * SSCCCCH  from outside to inside on the bottom and right border
	 * |||||||
	 * |||||||__ w_shin       (inner shadow area)
	 * ||||||___ w_c          (transparent area)
	 * |||||____ w_c          (transparent area)
	 * ||||_____ w_c          (transparent area)
	 * |||______ w_c          (transparent area)
	 * ||_______ w_hiout      (outer hilight area)
	 * |________ w_hiout      (outer hilight area)
	 *
	 *
	 * C = original colour
	 * H = hilight
	 * S = shadow
	 */
	ret_size_descr->w_dout = 0;
	ret_size_descr->w_hiout = 2;
	ret_size_descr->w_trout = 0;
	ret_size_descr->w_trin = 0;
	ret_size_descr->w_shin = 1;
	ret_size_descr->w_din = 0;
	ret_size_descr->sum = 3;
	ret_size_descr->trim = ret_size_descr->sum - fw->boundary_width + 1;
	check_remove_inset(borderstyle, ret_size_descr);
	trim_border_layout(fw, borderstyle, ret_size_descr);

	return;
}

static void border_fetch_fvwm_layout(
	FvwmWindow *fw, DecorFaceStyle *borderstyle,
	border_relief_size_descr *ret_size_descr)
{
	/* Fvwm borders look like this:
	 *
	 * SHHCCSS  from outside to inside on the left and top border
	 * SSCCHHS  from outside to inside on the bottom and right border
	 * |||||||
	 * |||||||__ w_din        (inner dark area)
	 * ||||||___ w_shin       (inner shadow area)
	 * |||||____ w_trin       (inner transparent/shadow area)
	 * ||||_____ w_c          (transparent area)
	 * |||______ w_trout      (outer transparent/hilight area)
	 * ||_______ w_hiout      (outer hilight area)
	 * |________ w_dout       (outer dark area)
	 *
	 * C = original colour
	 * H = hilight
	 * S = shadow
	 *
	 * reduced to 5 pixels it looks like this:
	 *
	 * SHHCS
	 * SSCHS
	 * |||||
	 * |||||__ w_din        (inner dark area)
	 * ||||___ w_trin       (inner transparent/shadow area)
	 * |||____ w_trout      (outer transparent/hilight area)
	 * ||_____ w_hiout      (outer hilight area)
	 * |______ w_dout       (outer dark area)
	 */
	ret_size_descr->w_dout = 1;
	ret_size_descr->w_hiout = 1;
	ret_size_descr->w_trout = 1;
	ret_size_descr->w_trin = 1;
	ret_size_descr->w_shin = 1;
	ret_size_descr->w_din = 1;
	/* w_trout + w_trin counts only as one pixel of border because
	 * they let one pixel of the original colour shine through. */
	ret_size_descr->sum = 6;
	ret_size_descr->trim = ret_size_descr->sum - fw->boundary_width;
	check_remove_inset(borderstyle, ret_size_descr);
	trim_border_layout(fw, borderstyle, ret_size_descr);

	return;
}

static void border_get_border_relief_size_descr(
	border_relief_size_descr *ret_size_descr, FvwmWindow *fw,
	Bool do_hilight)
{
	DecorFaceStyle *borderstyle;

	if (is_window_border_minimal(fw))
	{
		/* the border is too small, only a background but no relief */
		ret_size_descr->is_flat = 1;
		return;
	}
	borderstyle = (do_hilight) ?
		&GetDecor(fw, BorderStyle.active.style) :
		&GetDecor(fw, BorderStyle.inactive.style);
	if (borderstyle->flags.button_relief == DFS_BUTTON_IS_FLAT)
	{
		ret_size_descr->is_flat = 1;
		return;
	}
	ret_size_descr->is_flat = 0;
	/* get the relief layout */
	if (HAS_MWM_BORDER(fw))
	{
		border_fetch_mwm_layout(fw, borderstyle, ret_size_descr);
	}
	else
	{
		border_fetch_fvwm_layout(fw, borderstyle, ret_size_descr);
	}

	return;
}

static void border_get_border_marks_descr(
	common_decorations_type *cd, border_relief_descr *br, FvwmWindow *fw)
{
	int inset;

	/* get mark's length and thickness */
	inset = (br->relief.w_shin != 0 || br->relief.w_din != 0);
	br->marks.length = fw->boundary_width - br->relief.w_dout - inset;
	if (br->marks.length <= 0)
	{
		br->marks.has_x_marks = 0;
		br->marks.has_y_marks = 0;
		return;
	}
	br->marks.thickness = cd->relief_width;
	if (br->marks.thickness > br->marks.length)
	{
		br->marks.thickness = br->marks.length;
	}
	/* get offsets from outer side of window */
	br->marks.offset_tl = br->relief.w_dout;
	br->marks.offset_br =
		-br->relief.w_dout - br->marks.length - br->marks.offset_tl;

	return;
}

static Pixmap border_create_decor_pixmap(
	common_decorations_type *cd, rectangle *decor_g)
{
	Pixmap p;

	p = XCreatePixmap(
		dpy, Scr.Root, decor_g->width, decor_g->height, Pdepth);

	return p;
}

static void border_draw_part_relief(
	border_relief_descr *br, rectangle *frame_g, rectangle *part_g,
	Pixmap dest_pix, Bool is_inverted)
{
	int i;
	int off_x = 0;
	int off_y = 0;
	int width = frame_g->width - 1;
	int height = frame_g->height - 1;
	int w[7];
	GC gc[4];

	w[0] = br->relief.w_dout;
	w[1] = br->relief.w_hiout;
	w[2] = br->relief.w_trout;
	w[3] = br->relief.w_c;
	w[4] = br->relief.w_trin;
	w[5] = br->relief.w_shin;
	w[6] = br->relief.w_din;
	gc[(is_inverted == True)] = br->gcs.relief;
	gc[!(is_inverted == True)] = br->gcs.shadow;
	gc[2] = br->gcs.transparent;
	gc[3] = br->gcs.shadow;

	off_x = -part_g->x;
	off_y = -part_g->y;
	width = frame_g->width - 1;
	height = frame_g->height - 1;
	for (i = 0; i < 7; i++)
	{
		if (ulgc[i] != 0x7f && w[i] > 0)
		{
			RelieveRectangle(
				dpy, dest_pix, off_x, off_y,
				width, height, gc[(int)ulgc[i]],
				gc[(int)brgc[i]], w[i]);
		}
		off_x += w[i];
		off_y += w[i];
		width -= 2 * w[i];
		height -= 2 * w[i];
	}

	return;
}

static void border_draw_x_mark(
	border_relief_descr *br, int x, int y, Pixmap dest_pix,
	Bool do_draw_shadow)
{
	int k;
	int length;
	GC gc;

	if (br->marks.has_x_marks == 0)
	{
		return;
	}
	x += br->marks.offset_tl;
	gc = (do_draw_shadow) ? br->gcs.shadow : br->gcs.relief;
	/* draw it */
	for (k = 0, length = br->marks.length - 1; k < br->marks.thickness;
	     k++, length--)
	{
		int x1;
		int x2;
		int y1;
		int y2;

		if (length < 0)
		{
			break;
		}
		if (do_draw_shadow)
		{
			x1 = x + k;
			y1 = y - 1 - k;
		}
		else
		{
			x1 = x;
			y1 = y + k;
		}
		x2 = x1 + length;
		y2 = y1;
		XDrawLine(dpy, dest_pix, gc, x1, y1, x2, y2);
	}

	return;
}

static void border_draw_y_mark(
	border_relief_descr *br, int x, int y, Pixmap dest_pix,
	Bool do_draw_shadow)
{
	int k;
	int length;
	GC gc;

	if (br->marks.has_y_marks == 0)
	{
		return;
	}
	y += br->marks.offset_tl;
	gc = (do_draw_shadow) ? br->gcs.shadow : br->gcs.relief;
	/* draw it */
	for (k = 0, length = br->marks.length; k < br->marks.thickness;
	     k++, length--)
	{
		int x1;
		int x2;
		int y1;
		int y2;

		if (length <= 0)
		{
			break;
		}
		if (do_draw_shadow)
		{
			x1 = x - 1 - k;
			y1 = y + k;
		}
		else
		{
			x1 = x + k;
			y1 = y;
		}
		x2 = x1;
		y2 = y1 + length - 1;
		XDrawLine(dpy, dest_pix, gc, x1, y1, x2, y2);
	}

	return;
}

static void border_draw_part_marks(
	border_relief_descr *br, rectangle *part_g, window_parts part,
	Pixmap dest_pix)
{
	int l;
	int t;
	int w;
	int h;
	int o;

	l = br->sidebar_g.x;
	t = br->sidebar_g.y;
	w = part_g->width;
	h = part_g->height;
	o = br->marks.offset_br;
	switch (part)
	{
	case PART_BORDER_N:
		border_draw_y_mark(br, 0, 0, dest_pix, False);
		border_draw_y_mark(br, w, 0, dest_pix, True);
		break;
	case PART_BORDER_S:
		border_draw_y_mark(br, 0, h + o, dest_pix, False);
		border_draw_y_mark(br, w, h + o, dest_pix, True);
		break;
	case PART_BORDER_E:
		border_draw_x_mark(br, w + o, 0, dest_pix, False);
		border_draw_x_mark(br, w + o, h, dest_pix, True);
		break;
	case PART_BORDER_W:
		border_draw_x_mark(br, 0, 0, dest_pix, False);
		border_draw_x_mark(br, 0, h, dest_pix, True);
		break;
	case PART_BORDER_NW:
		border_draw_x_mark(br, 0, t, dest_pix, True);
		border_draw_y_mark(br, l, 0, dest_pix, True);
		break;
	case PART_BORDER_NE:
		border_draw_x_mark(br, l + o, t, dest_pix, True);
		border_draw_y_mark(br, 0, 0, dest_pix, False);
		break;
	case PART_BORDER_SW:
		border_draw_x_mark(br, 0, 0, dest_pix, False);
		border_draw_y_mark(br, l, t + o, dest_pix, True);
		break;
	case PART_BORDER_SE:
		border_draw_x_mark(br, l + o, 0, dest_pix, False);
		border_draw_y_mark(br, 0, t + o, dest_pix, False);
		break;
	default:
		return;
	}

	return;
}

inline static void border_set_part_background(
	Window w, Pixmap pix)
{
	XSetWindowAttributes xswa;

	xswa.background_pixmap = pix;
	XChangeWindowAttributes(dpy, w, CWBackPixmap, &xswa);

	return;
}

static void border_fill_pixmap_background(
	Pixmap dest_pix, rectangle *pixmap_g, pixmap_background_type *bg)
{
	static GC tile_gc = None;
	static GC mono_gc = None;
	Bool do_tile;
	XGCValues xgcv;
	unsigned long valuemask;

	do_tile = (bg->flags.use_pixmap && bg->pixmap.flags.is_tiled) ?
		True : False;
	if (tile_gc == None)
	{
		xgcv.fill_style = FillSolid;
		tile_gc = fvwmlib_XCreateGC(
			dpy, Scr.SizeWindow, GCFillStyle, &xgcv);
		if (tile_gc == None)
		{
			fvwm_msg(ERR, "border_fill_pixmap_background",
				 "Could not create border gc. exiting.");
			exit(1);
		}
	}
	if (mono_gc == None && do_tile && bg->pixmap.shape != None)
	{
		xgcv.fill_style = FillTiled;
		xgcv.clip_x_origin = 0;
		xgcv.clip_y_origin = 0;
		valuemask = GCFillStyle | GCClipXOrigin | GCClipYOrigin;
		mono_gc = fvwmlib_XCreateGC(
			dpy, bg->pixmap.shape, valuemask, &xgcv);
		if (mono_gc == None)
		{
			fvwm_msg(ERR, "border_fill_pixmap_background",
				 "Could not create border gc. exiting.");
			exit(1);
		}
	}

	if (!bg->flags.use_pixmap)
	{
		/* solid pixel */
		xgcv.fill_style = FillSolid;
		xgcv.foreground = bg->pixel;
		xgcv.clip_x_origin = 0;
		xgcv.clip_y_origin = 0;
		xgcv.clip_mask = None;
		valuemask = GCFillStyle | GCForeground | GCClipMask |
			GCClipXOrigin | GCClipYOrigin;
		XChangeGC(dpy, tile_gc, valuemask, &xgcv);
		XFillRectangle(
			dpy, dest_pix, tile_gc, 0, 0, pixmap_g->width,
			pixmap_g->height);
	}
	else if (do_tile == False)
	{
		/* pixmap, offset stored in pixmap_g->x/y */
		xgcv.fill_style = FillSolid;
		xgcv.clip_mask = bg->pixmap.shape;
		xgcv.clip_x_origin = pixmap_g->x;
		xgcv.clip_y_origin = pixmap_g->y;
		valuemask = GCFillStyle | GCClipMask | GCClipXOrigin |
			GCClipYOrigin;
		XChangeGC(dpy, tile_gc, valuemask, &xgcv);
		XCopyArea(
			dpy, bg->pixmap.p, dest_pix, tile_gc, 0, 0,
			bg->pixmap.size.width, bg->pixmap.size.height,
			pixmap_g->x, pixmap_g->y);
	}
	else
	{
		/* tiled pixmap */
		Pixmap shape = None;

		valuemask = 0;
		if (bg->pixmap.shape != None)
		{
			/* create a shape mask */
			shape = XCreatePixmap(
				dpy, Scr.Root, pixmap_g->width,
				pixmap_g->height, 1);
			xgcv.tile = bg->pixmap.shape;
			xgcv.ts_x_origin = -pixmap_g->x;
			xgcv.ts_y_origin = -pixmap_g->y;
			valuemask = GCTile | GCTileStipXOrigin |
				GCTileStipYOrigin;
			XChangeGC(dpy, mono_gc, valuemask, &xgcv);
			XFillRectangle(
				dpy, shape, mono_gc, 0, 0, pixmap_g->width,
				pixmap_g->height);
		}
		xgcv.clip_mask = shape;
		xgcv.fill_style = FillTiled;
		xgcv.tile = bg->pixmap.p;
		xgcv.ts_x_origin = -pixmap_g->x;
		xgcv.ts_y_origin = -pixmap_g->y;
		valuemask = GCFillStyle | GCClipMask | GCTile |
			GCTileStipXOrigin | GCTileStipYOrigin;
		XChangeGC(dpy, tile_gc, valuemask, &xgcv);
		XFillRectangle(
			dpy, dest_pix, tile_gc, 0, 0, pixmap_g->width,
			pixmap_g->height);
		if (shape != None)
		{
			XFreePixmap(dpy, shape);
		}
	}

	return;
}

static void border_get_border_background(
	pixmap_background_type *bg, common_decorations_type *cd)
{
	if (cd->valuemask & CWBackPixmap)
	{
		bg->flags.use_pixmap = 1;
		bg->pixmap.p = cd->attributes.background_pixmap;
		bg->pixmap.shape = None;
		bg->pixmap.flags.is_tiled = 1;
	}
	else
	{
		bg->flags.use_pixmap = 0;
		bg->pixel = cd->attributes.background_pixel;
	}

	return;
}

static void border_draw_one_border_part(
	common_decorations_type *cd, FvwmWindow *fw, rectangle *sidebar_g,
	rectangle *frame_g, border_relief_descr *br, window_parts part,
	window_parts draw_handles, Bool is_inverted, Bool do_hilight,
	Bool do_clear)
{
	pixmap_background_type bg;
	rectangle part_g;
	Pixmap p;
	Window w;

	/* make a pixmap */
	border_get_part_geometry(fw, part, sidebar_g, &part_g, &w);
	if (part_g.width <= 0 || part_g.height <= 0)
	{
		return;
	}
	p = border_create_decor_pixmap(cd, &part_g);
	/* set the background tile */
	border_get_border_background(&bg, cd);
print_g("filling:", &part_g);
	border_fill_pixmap_background(p, &part_g, &bg);
	/* draw the relief over the background */
	if (!br->relief.is_flat)
	{
		border_draw_part_relief(br, frame_g, &part_g, p, is_inverted);
	}
	/* draw the handle marks */
	if (br->marks.has_x_marks || br->marks.has_y_marks)
	{
		border_draw_part_marks(br, &part_g, part, p);
	}
	/* apply the pixmap and destroy it */
	border_set_part_background(w, p);
	if (do_clear == True)
	{
		XClearWindow(dpy,w);
	}
	XFreePixmap(dpy, p);

	return;
}

static void border_draw_all_border_parts(
        common_decorations_type *cd, FvwmWindow *fw, border_relief_descr *br,
	rectangle *frame_g, window_parts draw_parts,
	window_parts pressed_parts, Bool do_hilight, Bool do_clear)
{
	window_parts part;
	window_parts draw_handles;

	/* get the description of the drawing directives */
	border_get_border_relief_size_descr(&br->relief, fw, do_hilight);
	border_get_border_marks_descr(cd, br, fw);
	/* fetch the gcs used to draw the border */
	border_get_border_gcs(&br->gcs, cd, fw, do_hilight);
	/* draw everything in a big loop */
	draw_parts &= (PART_FRAME | PART_HANDLES);
	draw_handles = (draw_parts & PART_HANDLES);
fprintf(stderr, "drawing border parts 0x%04x\n", draw_parts);
	for (part = PART_BORDER_N; (part & PART_FRAME); part <<= 1)
	{
		if (part & draw_parts)
		{
			border_draw_one_border_part(
				cd, fw, &br->sidebar_g, frame_g, br, part,
				draw_handles,
				(pressed_parts & part) ? True : False,
				do_hilight, do_clear);
		}
	}
	/* update the border states */
	fw->decor_state.parts_drawn |= draw_parts;
	if (do_hilight)
	{
		fw->decor_state.parts_lit |= draw_parts;
	}
	fw->decor_state.parts_inverted &= ~draw_parts;
	fw->decor_state.parts_inverted |= (draw_parts & pressed_parts);

	return;
}

/****************************************************************************
 *
 *  Draws a little pattern within a window (more complex)
 *
 ****************************************************************************/
static void border_draw_vector_to_pixmap(
	Pixmap dest_pix, common_decorations_type *cd, int is_toggled,
	struct vector_coords *coords, rectangle *pixmap_g)
{
	GC gcs[4];
	int i;

	if (coords->use_fgbg == 1)
	{
		Globalgcv.foreground = cd->fore_color;
		Globalgcm = GCForeground;
		XChangeGC(dpy, Scr.ScratchGC3, Globalgcm, &Globalgcv);
		Globalgcv.foreground = cd->back_color;
		XChangeGC(dpy, Scr.ScratchGC4, Globalgcm, &Globalgcv);
		gcs[2] = Scr.ScratchGC3;
		gcs[3] = Scr.ScratchGC4;
	}
	if (is_toggled)
	{
		gcs[0] = cd->relief_gc;
		gcs[1] = cd->shadow_gc;
	}
	else
	{
		gcs[0] = cd->shadow_gc;
		gcs[1] = cd->relief_gc;
	}
	for (i = 1; i < coords->num; i++)
	{
		if (coords->c[i] < 0 || coords->c[i] >= 4)
		{
			/* don't draw a line */
			continue;
		}
		XDrawLine(
			dpy, dest_pix, gcs[coords->c[i]],
			pixmap_g->width * coords->x[i-1] / 100,
			pixmap_g->height * coords->y[i-1] / 100,
			pixmap_g->width * coords->x[i] / 100,
			pixmap_g->height * coords->y[i] / 100);
	}

	return;
}


static void border_draw_decor_to_pixmap(
	FvwmWindow *fw, Pixmap dest_pix, pixmap_background_type *solid_bg,
	rectangle *pixmap_g, DecorFace *df, common_decorations_type *cd,
	int is_toggled, int left1right0)
{
	register DecorFaceType type = DFS_FACE_TYPE(df->style);
	pixmap_background_type bg;
	rectangle r;
	Picture *p;
	int border;

	switch (type)
	{
	case SimpleButton:
		/* do nothing */
		break;
	case SolidButton:
		/* overwrite with the default background */
		border_fill_pixmap_background(dest_pix, pixmap_g, solid_bg);
		break;
	case VectorButton:
	case DefaultVectorButton:
		border_draw_vector_to_pixmap(
			dest_pix, cd, is_toggled, &df->u.vector, pixmap_g);
		break;
	case MiniIconButton:
	case PixmapButton:
		if (FMiniIconsSupported && type == MiniIconButton)
		{
			if (!fw->mini_icon)
			{
				break;
			}
			p = fw->mini_icon;
		}
		else
		{
			p = df->u.p;
		}
		if (DFS_BUTTON_RELIEF(df->style) == DFS_BUTTON_IS_FLAT)
		{
			border = 0;
		}
		else
		{
			border = HAS_MWM_BORDER(fw) ? 1 : 2;
		}
		r = *pixmap_g;
		switch (DFS_H_JUSTIFICATION(df->style))
		{
		case JUST_LEFT:
			r.x = border;
			break;
		case JUST_RIGHT:
			r.x = (int)(pixmap_g->width - p->width - border);
			break;
		case JUST_CENTER:
		default:
			/* round down */
			r.x += (int)(pixmap_g->width - p->width) / 2;
			break;
		}
		switch (DFS_H_JUSTIFICATION(df->style))
		{
		case JUST_TOP:
			r.y = border;
			break;
		case JUST_BOTTOM:
			r.y = (int)(pixmap_g->height - p->height - border);
			break;
		case JUST_CENTER:
		default:
			/* round down */
			r.y += (int)(pixmap_g->height - p->height) / 2;
			break;
		}
		if (r.x < border)
		{
			r.x = border;
		}
		if (r.y < border)
		{
			r.y = border;
		}
		bg.flags.use_pixmap = 1;
		bg.pixmap.p = p->picture;
		bg.pixmap.shape = p->mask;
		bg.pixmap.size.width = p->width;
		bg.pixmap.size.height = p->height;
		bg.pixmap.flags.is_tiled = 0;
		border_fill_pixmap_background(dest_pix, &r, &bg);
		break;
	case TiledPixmapButton:
#ifdef FANCY_TITLEBARS
	case MultiPixmap: /* in case of UseTitleStyle */
#endif
		if (type == TiledPixmapButton)
		{
			p = df->u.p;
		}
#ifdef FANCY_TITLEBARS
		else
		{
			if (left1right0 &&
			    df->u.multi_pixmaps[TBP_LEFT_BUTTONS])
			{
				p = df->u.multi_pixmaps[TBP_LEFT_BUTTONS];
			}
			else if (!left1right0 &&
				 df->u.multi_pixmaps[TBP_RIGHT_BUTTONS])
			{
				p = df->u.multi_pixmaps[TBP_RIGHT_BUTTONS];
			}
			else if (df->u.multi_pixmaps[TBP_BUTTONS])
			{
				p = df->u.multi_pixmaps[TBP_BUTTONS];
			}
			else if (left1right0 &&
				 df->u.multi_pixmaps[TBP_LEFT_MAIN])
			{
				p = df->u.multi_pixmaps[TBP_LEFT_MAIN];
			}
			else if (!left1right0 &&
				 df->u.multi_pixmaps[TBP_RIGHT_MAIN])
			{
				p = df->u.multi_pixmaps[TBP_RIGHT_MAIN];
			}
			else
			{
				p = df->u.multi_pixmaps[TBP_MAIN];
			}
		}
#endif
		if (DFS_BUTTON_RELIEF(df->style) == DFS_BUTTON_IS_FLAT)
		{
			border = 0;
		}
		else
		{
			border = HAS_MWM_BORDER(fw) ? 1 : 2;
		}
		r = *pixmap_g;
		r.x = border;
		r.y = border;
		bg.flags.use_pixmap = 1;
		bg.pixmap.p = p->picture;
		bg.pixmap.shape = p->mask;
		bg.pixmap.flags.is_tiled = 1;
		border_fill_pixmap_background(dest_pix, &r, &bg);
		break;
	case GradientButton:
		/* draw the gradient into the pixmap */
		CreateGradientPixmap(
			dpy, dest_pix, Scr.TransMaskGC,
			df->u.grad.gradient_type, 0, 0, df->u.grad.npixels,
			df->u.grad.pixels, dest_pix, 0, 0, pixmap_g->width,
			pixmap_g->height, NULL);
		break;
	default:
		fvwm_msg(ERR, "DrawButton", "unknown button type");
		break;
	}

	return;
}

static void border_set_button_pixmap(
	FvwmWindow *fw, titlebar_descr *td, int button, Pixmap dest_pix)
{
	pixmap_background_type bg;
	unsigned int mask;
	int is_left_button;
	int do_reverse_relief;
	ButtonState bs;
	DecorFace *df;
	rectangle *button_g;
	GC rgc;
	GC sgc;

	/* prepare variables */
	mask = (1 << button);
	is_left_button = !(button & 1);
	button_g = &td->layout.button_g[button];
	bs = td->tbstate.bstate[button];
	df = &TB_STATE(GetDecor(fw, buttons[button]))[bs];
	rgc = td->cd->relief_gc;
	sgc = td->cd->shadow_gc;
	/* prepare background, either from the window colour or from the
	 * border style */
	if (!DFS_USE_BORDER_STYLE(df->style))
	{
		/* fill with the button background colour */
		bg.flags.use_pixmap = 0;
		bg.pixel = td->cd->back_color;
	}
	else
	{
		/* draw pixmap background inherited from border style */
		border_get_border_background(&bg, td->cd);
	}
	border_fill_pixmap_background(dest_pix, button_g, &bg);
	/* handle title style */
	if (DFS_USE_TITLE_STYLE(df->style))
	{
		/* draw background inherited from title style */
		DecorFace *tsdf;

		for (tsdf = &TB_STATE(GetDecor(fw, titlebar))[bs];
		     tsdf != NULL; tsdf = tsdf->next)
		{
			border_draw_decor_to_pixmap(
				fw, dest_pix, &bg, button_g, tsdf, td->cd,
				(td->tbstate.toggled_bmask & mask),
				is_left_button);
		}
	}
	/* handle button style */
	for ( ; df; df = df->next)
	{
		/* draw background from button style */
		border_draw_decor_to_pixmap(
			fw, dest_pix, &bg, button_g, df, td->cd,
			(td->tbstate.toggled_bmask & mask), is_left_button);
	}
	/* draw the button relief */
	do_reverse_relief = !!(td->tbstate.pressed_bmask & mask);
	switch (DFS_BUTTON_RELIEF(
			TB_STATE(GetDecor(fw, buttons[button]))[bs].style))
	{
	case DFS_BUTTON_IS_SUNK:
		do_reverse_relief ^= 1;
		/* fall through*/
	case DFS_BUTTON_IS_UP:
		RelieveRectangle2(
			dpy, dest_pix, 0, 0, button_g->width - 1,
			button_g->height - 1,
			(do_reverse_relief) ? sgc : rgc,
			(do_reverse_relief) ? rgc : sgc,
			td->cd->relief_width);
		break;
	default:
		/* flat */
		break;
	}

	return;
}

static void border_draw_one_button(
	FvwmWindow *fw, titlebar_descr *td, int button)
{
	Pixmap p;

	/* make a pixmap */
	if (td->layout.button_g[button].x < 0 ||
	    td->layout.button_g[button].y < 0)
	{
		return;
	}
	p = border_create_decor_pixmap(td->cd, &(td->layout.button_g[button]));
	/* set the background tile */
	border_set_button_pixmap(fw, td, button, p);
	/* apply the pixmap and destroy it */
	border_set_part_background(FW_W_BUTTON(fw, button), p);
	if ((td->tbstate.clear_bmask & (1 << button)) != 0)
	{
		XClearWindow(dpy, FW_W_BUTTON(fw, button));
	}
	XFreePixmap(dpy, p);

	return;
}

static void border_draw_title(
	FvwmWindow *fw, titlebar_descr *td)
{
	/*!!!*/

	return;
}

static void border_draw_buttons(
	FvwmWindow *fw, titlebar_descr *td)
{
	int i;

	/* draw everything in a big loop */
fprintf(stderr, "drawing buttons 0x%04x\n", td->tbstate.draw_bmask);
	for (i = 0; i < NUMBER_OF_BUTTONS; i++)
	{
		unsigned int mask = (1 << i);

		if ((td->tbstate.draw_bmask & mask) != 0)
		{
			border_draw_one_button(fw, td, i);
		}
	}
	/* update the button states */
	fw->decor_state.buttons_drawn |= td->tbstate.draw_bmask;
	fw->decor_state.buttons_inverted = td->tbstate.pressed_bmask;
	fw->decor_state.buttons_lit = td->tbstate.lit_bmask;
	fw->decor_state.buttons_toggled = td->tbstate.toggled_bmask;

	return;
}

static window_parts border_get_titlebar_descr(
	common_decorations_type *cd, FvwmWindow *fw,
	window_parts pressed_parts, int pressed_button,
	window_parts force_draw_parts, clear_window_parts clear_parts,
	rectangle *old_g, rectangle *new_g, Bool do_hilight,
	titlebar_descr *ret_td)
{
	window_parts draw_parts;
	int i;

	ret_td->cd = cd;
	ret_td->frame_g = *new_g;
	if (old_g == NULL)
	{
		old_g = &fw->frame_g;
	}
	frame_get_title_bar_dimensions(fw, old_g, NULL, &ret_td->old_layout);
	frame_get_title_bar_dimensions(fw, new_g, NULL, &ret_td->layout);
	/* initialise flags */
	if ((pressed_parts & PART_BUTTONS) != PART_NONE && pressed_button >= 0)
	{
		ret_td->tbstate.pressed_bmask = (1 << pressed_button);
	}
	else
	{
		ret_td->tbstate.pressed_bmask = 0;
	}
	if ((clear_parts & CLEAR_BUTTONS) != CLEAR_NONE)
	{
		ret_td->tbstate.clear_bmask = ~0;
	}
	else
	{
		ret_td->tbstate.clear_bmask = 0;
	}
	ret_td->tbstate.lit_bmask = (do_hilight == True) ? ~0 : 0;
	if ((pressed_parts & PART_TITLE) != PART_NONE)
	{
		ret_td->tbstate.is_title_pressed = 1;
	}
	else
	{
		ret_td->tbstate.is_title_pressed = 0;
	}
	if ((clear_parts & CLEAR_TITLE) != CLEAR_NONE)
	{
		ret_td->tbstate.do_clear_title = 1;
	}
	else
	{
		ret_td->tbstate.do_clear_title = 0;
	}
	ret_td->tbstate.is_title_lit = (do_hilight == True) ? 1 : 0;
	ret_td->tbstate.toggled_bmask = 0;
	for (i = 0; i < NUMBER_OF_BUTTONS; i++)
	{
		unsigned int mask = (1 << i);

		if (is_button_toggled(fw, i))
		{
			ret_td->tbstate.toggled_bmask |= mask;
		}
		ret_td->tbstate.bstate[i] = border_flags_to_button_state(
			ret_td->tbstate.pressed_bmask & mask,
			ret_td->tbstate.lit_bmask & mask,
			ret_td->tbstate.toggled_bmask & mask);
	}
	ret_td->tbstate.is_title_toggled = (is_title_toggled(fw) == True);
	ret_td->tbstate.tstate = border_flags_to_button_state(
		ret_td->tbstate.is_title_pressed, ret_td->tbstate.is_title_lit,
		ret_td->tbstate.is_title_toggled);
#if 0
	/*!!!*/
	draw_titlebar_gcs gcs;
#endif
	/* determine the parts to draw */
	draw_parts = border_get_tb_parts_to_draw(fw, ret_td, force_draw_parts);

	return draw_parts;
}


static void border_draw_titlebar(
	common_decorations_type *cd, FvwmWindow *fw,
	window_parts pressed_parts, int pressed_button,
	window_parts force_draw_parts, clear_window_parts clear_parts,
	rectangle *old_g, rectangle *new_g, Bool do_hilight)
{
	window_parts draw_parts;
	titlebar_descr td;

	if (!HAS_TITLE(fw))
	{
		/* just reset border states */
		fw->decor_state.parts_drawn &= ~(PART_TITLE);
		fw->decor_state.parts_lit &= ~(PART_TITLE);
		fw->decor_state.parts_inverted &= ~(PART_TITLE);
		fw->decor_state.parts_toggled &= ~(PART_TITLE);
		fw->decor_state.buttons_drawn = 0;
		fw->decor_state.buttons_lit = 0;
		fw->decor_state.buttons_inverted = 0;
		fw->decor_state.buttons_toggled = 0;
		return;
	}
	memset(&td, 0, sizeof(td));
	draw_parts = border_get_titlebar_descr(
		cd, fw, pressed_parts, pressed_button, force_draw_parts,
		clear_parts, old_g, new_g, do_hilight, &td);
	if ((draw_parts & PART_TITLE) != PART_NONE)
	{
		border_draw_title(fw, &td);
	}
	if ((draw_parts & PART_BUTTONS) != PART_NONE)
	{
		border_draw_buttons(fw, &td);
	}

	return;
}

/****************************************************************************
 *
 * Redraws the windows borders
 *
 ****************************************************************************/
static void border_draw_border_parts(
	common_decorations_type *cd, FvwmWindow *fw,
	window_parts pressed_parts, window_parts force_draw_parts,
	clear_window_parts clear_parts, rectangle *old_g, rectangle *new_g,
	Bool do_hilight)
{
	border_relief_descr br;
	window_parts draw_parts;
	Bool do_clear;

	if (!HAS_BORDER(fw))
	{
		/* just reset border states */
		fw->decor_state.parts_drawn &= ~(PART_FRAME | PART_HANDLES);
		fw->decor_state.parts_lit &= ~(PART_FRAME | PART_HANDLES);
		fw->decor_state.parts_inverted &= ~(PART_FRAME | PART_HANDLES);
		fw->decor_state.parts_toggled &= ~(PART_FRAME | PART_HANDLES);
		return;
	}
	do_clear = (clear_parts & CLEAR_FRAME) ? True : False;
	/* determine the parts to draw and the position to place them */
	if (HAS_DEPRESSABLE_BORDER(fw))
	{
		pressed_parts &= PART_FRAME;
	}
	else
	{
		pressed_parts = PART_NONE;
	}
	force_draw_parts &= PART_FRAME;
	memset(&br, 0, sizeof(br));
	draw_parts = border_get_parts_and_pos_to_draw(
		cd, fw, pressed_parts, force_draw_parts, old_g, new_g,
		do_hilight, &br);
	if ((draw_parts & PART_FRAME) != PART_NONE)
	{
		border_draw_all_border_parts(
			cd, fw, &br, new_g, draw_parts, pressed_parts,
			do_hilight, do_clear);
	}

	return;
}

/* ---------------------------- interface functions ------------------------- */

void border_get_part_geometry(
	FvwmWindow *fw, window_parts part, rectangle *sidebar_g,
	rectangle *ret_g, Window *ret_w)
{
	int bw;

	bw = fw->boundary_width;
	switch (part)
	{
	case PART_BORDER_N:
		ret_g->x = sidebar_g->x;
		ret_g->y = 0;
		*ret_w = FW_W_SIDE(fw, 0);
		break;
	case PART_BORDER_E:
		ret_g->x = 2 * sidebar_g->x + sidebar_g->width - bw;
		ret_g->y = sidebar_g->y;
		*ret_w = FW_W_SIDE(fw, 1);
		break;
	case PART_BORDER_S:
		ret_g->x = sidebar_g->x;
		ret_g->y = 2 * sidebar_g->y + sidebar_g->height - bw;
		*ret_w = FW_W_SIDE(fw, 2);
		break;
	case PART_BORDER_W:
		ret_g->x = 0;
		ret_g->y = sidebar_g->y;
		*ret_w = FW_W_SIDE(fw, 3);
		break;
	case PART_BORDER_NW:
		ret_g->x = 0;
		ret_g->y = 0;
		*ret_w = FW_W_CORNER(fw, 0);
		break;
	case PART_BORDER_NE:
		ret_g->x = sidebar_g->x + sidebar_g->width;
		ret_g->y = 0;
		*ret_w = FW_W_CORNER(fw, 1);
		break;
	case PART_BORDER_SW:
		ret_g->x = 0;
		ret_g->y = sidebar_g->y + sidebar_g->height;
		*ret_w = FW_W_CORNER(fw, 2);
		break;
	case PART_BORDER_SE:
		ret_g->x = sidebar_g->x + sidebar_g->width;;
		ret_g->y = sidebar_g->y + sidebar_g->height;
		*ret_w = FW_W_CORNER(fw, 3);
		break;
	default:
		return;
	}
	switch (part)
	{
	case PART_BORDER_N:
	case PART_BORDER_S:
		ret_g->width = sidebar_g->width;
		ret_g->height = bw;
		break;
	case PART_BORDER_E:
	case PART_BORDER_W:
		ret_g->width = bw;
		ret_g->height = sidebar_g->height;
		break;
	case PART_BORDER_NW:
	case PART_BORDER_NE:
	case PART_BORDER_SW:
	case PART_BORDER_SE:
		ret_g->width = sidebar_g->x;
		ret_g->height = sidebar_g->y;
		break;
	default:
		return;
	}

	return;
}

int get_button_number(int context)
{
	int i;

	for (i = 0; (C_L1 << i) & (C_LALL | C_RALL); i++)
	{
		if (context & (C_L1 << i))
		{
			return i;
		}
	}

	return -1;
}

void draw_decorations_with_geom(
	FvwmWindow *fw, window_parts draw_parts, Bool has_focus, int force,
	clear_window_parts clear_parts, rectangle *old_g, rectangle *new_g)
{
	common_decorations_type cd;
	Bool is_titlebar_redraw_allowed = False;
	Bool do_redraw_titlebar = False;
	window_parts pressed_parts;
	window_parts force_parts;
	int context;
	int item;

	if (!fw)
	{
		return;
	}
	memset(&cd, 0, sizeof(cd));
	if (force)
	{
		is_titlebar_redraw_allowed = True;
	}
	if (has_focus)
	{
		/* don't re-draw just for kicks */
		if (!force && Scr.Hilite == fw)
		{
			is_titlebar_redraw_allowed = False;
		}
		else
		{
			if (Scr.Hilite != fw || force > 1)
			{
				cd.flags.has_color_changed = True;
			}
			if (Scr.Hilite != fw && Scr.Hilite != NULL)
			{
				/* make sure that the previously highlighted
				 * window got unhighlighted */
				DrawDecorations(
					Scr.Hilite, PART_ALL, False, True,
					None, CLEAR_ALL);
			}
			Scr.Hilite = fw;
		}
	}
	else
	{
		/* don't re-draw just for kicks */
		if (!force && Scr.Hilite != fw)
		{
			is_titlebar_redraw_allowed = False;
		}
		else if (Scr.Hilite == fw || force > 1)
		{
			cd.flags.has_color_changed = True;
			Scr.Hilite = NULL;
		}
	}
	if (IS_ICONIFIED(fw))
	{
		DrawIconWindow(fw);
		return;
	}

	/* calculate some values and flags */
	if ((draw_parts & PART_TITLEBAR) && HAS_TITLE(fw) &&
	    is_titlebar_redraw_allowed)
	{
		do_redraw_titlebar = True;
	}
	if (cd.flags.has_color_changed && HAS_TITLE(fw))
	{
		do_redraw_titlebar = True;
	}
	get_common_decorations(
		&cd, fw, draw_parts, has_focus, False, do_redraw_titlebar);

	/* redraw */
	context = frame_window_id_to_context(fw, PressedW, &item);
	if ((context & (C_LALL | C_RALL)) == 0)
	{
		item = -1;
	}
	pressed_parts = border_context_to_parts(context);
	if (do_redraw_titlebar)
	{
		force_parts = (force) ?
			(draw_parts & PART_TITLEBAR) : PART_NONE;
		border_draw_titlebar(
			&cd, fw, draw_parts, item, force_parts, clear_parts,
			old_g, new_g, has_focus);
	}
	if (cd.flags.has_color_changed || (draw_parts & PART_FRAME))
	{
		get_common_decorations(
			&cd, fw, draw_parts, has_focus, True, True);
		force_parts = (force) ? (draw_parts & PART_FRAME) : PART_NONE;
		border_draw_border_parts(
			&cd, fw, pressed_parts, force_parts, clear_parts,
			old_g, new_g, has_focus);
	}

	return;
}

void DrawDecorations(
	FvwmWindow *fw, window_parts draw_parts, Bool has_focus, int force,
	Window expose_win, clear_window_parts clear_parts)
{
	draw_decorations_with_geom(
		fw, draw_parts, has_focus, force, clear_parts, NULL,
		&fw->frame_g);

	return;
}

/****************************************************************************
 *
 *  redraw the decoration when style change
 *
 ****************************************************************************/
void RedrawDecorations(FvwmWindow *fw)
{
	FvwmWindow *u = Scr.Hilite;

	/* domivogt (6-Jun-2000): Don't check if the window is visible here.
	 * If we do, some updates are not applied and when the window becomes
	 * visible again, the X Server may not redraw the window. */
	DrawDecorations(fw, PART_ALL, (Scr.Hilite == fw), 2, None, CLEAR_ALL);
	Scr.Hilite = u;

	return;
}

/* ---------------------------- builtin commands ---------------------------- */

/****************************************************************************
 *
 *  Sets the allowed button states
 *
 ****************************************************************************/
void CMD_ButtonState(F_CMD_ARGS)
{
	char *token;

	while ((token = PeekToken(action, &action)))
	{
		static char first = True;
		if (!token && first)
		{
			Scr.gs.use_active_down_buttons =
				DEFAULT_USE_ACTIVE_DOWN_BUTTONS;
			Scr.gs.use_inactive_buttons =
				DEFAULT_USE_INACTIVE_BUTTONS;
			return;
		}
		first = False;
		if (StrEquals("activedown", token))
		{
			Scr.gs.use_active_down_buttons = ParseToggleArgument(
				action, &action,
				DEFAULT_USE_ACTIVE_DOWN_BUTTONS, True);
		}
		else if (StrEquals("inactive", token))
		{
			Scr.gs.use_inactive_buttons = ParseToggleArgument(
				action, &action,
				DEFAULT_USE_ACTIVE_DOWN_BUTTONS, True);
		}
		else
		{
			Scr.gs.use_active_down_buttons =
				DEFAULT_USE_ACTIVE_DOWN_BUTTONS;
			Scr.gs.use_inactive_buttons =
				DEFAULT_USE_INACTIVE_BUTTONS;
			fvwm_msg(ERR, "cmd_button_state",
				 "unknown button state %s\n", token);
			return;
		}
	}

	return;
}

/****************************************************************************
 *
 *  Sets the border style (veliaa@rpi.edu)
 *
 ****************************************************************************/
void CMD_BorderStyle(F_CMD_ARGS)
{
	char *parm;
	char *prev;
#ifdef USEDECOR
	FvwmDecor *decor = Scr.cur_decor ? Scr.cur_decor : &Scr.DefaultDecor;
#else
	FvwmDecor *decor = &Scr.DefaultDecor;
#endif

	Scr.flags.do_need_window_update = 1;
	decor->flags.has_changed = 1;

	for (prev = action; (parm = PeekToken(action, &action)); prev = action)
	{
		if (StrEquals(parm, "active") || StrEquals(parm, "inactive"))
		{
			int len;
			char *end, *tmp;
			DecorFace tmpdf, *df;
			memset(&tmpdf.style, 0, sizeof(tmpdf.style));
			DFS_FACE_TYPE(tmpdf.style) = SimpleButton;
			tmpdf.next = NULL;
                        if (FMiniIconsSupported)
                        {
                                tmpdf.u.p = NULL;
                        }
			if (StrEquals(parm,"active"))
			{
				df = &decor->BorderStyle.active;
			}
			else
			{
				df = &decor->BorderStyle.inactive;
			}
			df->flags.has_changed = 1;
			while (isspace(*action))
				++action;
			if (*action != '(')
			{
				if (!*action)
				{
					fvwm_msg(
						ERR, "SetBorderStyle",
						"error in %s border"
						" specification", parm);
					return;
				}
				while (isspace(*action))
					++action;
				if (ReadDecorFace(action, &tmpdf,-1,True))
				{
					FreeDecorFace(dpy, df);
					*df = tmpdf;
				}
				break;
			}
			end = strchr(++action, ')');
			if (!end)
			{
				fvwm_msg(
					ERR, "SetBorderStyle",
					"error in %s border specification",
					parm);
				return;
			}
			len = end - action + 1;
			tmp = safemalloc(len);
			strncpy(tmp, action, len - 1);
			tmp[len - 1] = 0;
			ReadDecorFace(tmp, df,-1,True);
			free(tmp);
			action = end + 1;
		}
		else if (strcmp(parm,"--")==0)
		{
			if (ReadDecorFace(
				    prev, &decor->BorderStyle.active,-1,True))
			{
				ReadDecorFace(
					prev, &decor->BorderStyle.inactive, -1,
					False);
			}
			decor->BorderStyle.active.flags.has_changed = 1;
			decor->BorderStyle.inactive.flags.has_changed = 1;
			break;
		}
		else
		{
			DecorFace tmpdf;
			memset(&tmpdf.style, 0, sizeof(tmpdf.style));
			DFS_FACE_TYPE(tmpdf.style) = SimpleButton;
			tmpdf.next = NULL;
                        if (FMiniIconsSupported)
                        {
                                tmpdf.u.p = NULL;
                        }
			if (ReadDecorFace(prev, &tmpdf,-1,True))
			{
				FreeDecorFace(dpy,&decor->BorderStyle.active);
				decor->BorderStyle.active = tmpdf;
				ReadDecorFace(
					prev, &decor->BorderStyle.inactive, -1,
					False);
				decor->BorderStyle.active.flags.has_changed = 1;
				decor->BorderStyle.inactive.flags.has_changed =
					1;
			}
			break;
		}
	}

	return;
}
