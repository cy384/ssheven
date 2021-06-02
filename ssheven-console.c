/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#include "ssheven.h"
#include "ssheven-console.h"
#include "ssheven-net.h"

#include <string.h>

#include <Sound.h>

#include <vterm.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

char key_to_vterm[256] = { VTERM_KEY_NONE };

int font_offset = 0;

void setup_key_translation(void)
{
	// TODO: figure out how to translate the rest of these
	key_to_vterm[kEnterCharCode] = VTERM_KEY_ENTER;
	key_to_vterm[kTabCharCode] = VTERM_KEY_TAB;
	key_to_vterm[kBackspaceCharCode] = VTERM_KEY_BACKSPACE;
	key_to_vterm[kEscapeCharCode] = VTERM_KEY_ESCAPE;
	key_to_vterm[kUpArrowCharCode] = VTERM_KEY_UP;
	key_to_vterm[kDownArrowCharCode] = VTERM_KEY_DOWN;
	key_to_vterm[kLeftArrowCharCode] = VTERM_KEY_LEFT;
	key_to_vterm[kRightArrowCharCode] = VTERM_KEY_RIGHT;
	//key_to_vterm[0] = VTERM_KEY_INS;
	key_to_vterm[kDeleteCharCode] = VTERM_KEY_DEL;
	key_to_vterm[kHomeCharCode] = VTERM_KEY_HOME;
	key_to_vterm[kEndCharCode] = VTERM_KEY_END;
	key_to_vterm[kPageUpCharCode] = VTERM_KEY_PAGEUP;
	key_to_vterm[kPageDownCharCode] = VTERM_KEY_PAGEDOWN;
	//key_to_vterm[0] = VTERM_KEY_KP_0;
	//key_to_vterm[0] = VTERM_KEY_KP_1;
	//key_to_vterm[0] = VTERM_KEY_KP_2;
	//key_to_vterm[0] = VTERM_KEY_KP_3;
	//key_to_vterm[0] = VTERM_KEY_KP_4;
	//key_to_vterm[0] = VTERM_KEY_KP_5;
	//key_to_vterm[0] = VTERM_KEY_KP_6;
	//key_to_vterm[0] = VTERM_KEY_KP_7;
	//key_to_vterm[0] = VTERM_KEY_KP_8;
	//key_to_vterm[0] = VTERM_KEY_KP_9;
	//key_to_vterm[0] = VTERM_KEY_KP_MULT;
	//key_to_vterm[0] = VTERM_KEY_KP_PLUS;
	//key_to_vterm[0] = VTERM_KEY_KP_COMMA;
	//key_to_vterm[0] = VTERM_KEY_KP_MINUS;
	//key_to_vterm[0] = VTERM_KEY_KP_PERIOD;
	//key_to_vterm[0] = VTERM_KEY_KP_DIVIDE;
	//key_to_vterm[0] = VTERM_KEY_KP_ENTER;
	//key_to_vterm[0] = VTERM_KEY_KP_EQUAL;
}

inline Rect cell_rect(int x, int y, Rect bounds)
{
	Rect r = { (short) (bounds.top + y * con.cell_height + 2), (short) (bounds.left + x * con.cell_width + 2),
		(short) (bounds.top + (y+1) * con.cell_height + 2), (short) (bounds.left + (x+1) * con.cell_width + 2) };
	return r;
}

void print_string(const char* c)
{
	vterm_input_write(con.vterm, c, strlen(c));
}

inline void draw_char(int x, int y, Rect* r, char c)
{
	MoveTo(r->left + 2 + x * con.cell_width, r->top + 2 - font_offset + ((y+1) * con.cell_height));
	DrawChar(c);
}

void toggle_cursor(void)
{
	con.cursor_state = !con.cursor_state;
	Rect cursor = cell_rect(con.cursor_x, con.cursor_y, con.win->portRect);
	InvalRect(&cursor);
}

void check_cursor(void)
{
	long unsigned int now = TickCount();
	if ((now - con.last_cursor_blink) > GetCaretTime())
	{
		toggle_cursor();
		con.last_cursor_blink = now;
	}
}

// convert Quickdraw colors into vterm's ANSI color indexes
int qd2idx(int qdc)
{
	switch (qdc)
	{
		case blackColor: return 0;
		case redColor: return 1;
		case greenColor: return 2;
		case yellowColor: return 3;
		case blueColor: return 4;
		case magentaColor: return 5;
		case cyanColor: return 6;
		case whiteColor: return 7;
		default: return 0;
	}
}

// convert vterm's ANSI color indexes into Quickdraw colors
int idx2qd[16] = { blackColor, redColor, greenColor, yellowColor, blueColor, magentaColor, cyanColor, whiteColor, blackColor, redColor, greenColor, yellowColor, blueColor, magentaColor, cyanColor, whiteColor };

void point_to_cell(Point p, int* x, int* y)
{
	*x = p.h / con.cell_width;
	*y = p.v / con.cell_height;

	if (*x > con.size_x) *x = con.size_x;
	if (*y > con.size_y) *y = con.size_y;
}

void clear_selection(void)
{
	con.select_start_x = -1;
	con.select_start_y = -1;
	con.select_end_x = -1;
	con.select_end_y = -1;
}

void damage_selection(void)
{
	// damage all rows that have part of the selection (TODO make this better)
	Rect topleft = cell_rect(0, MIN(con.select_start_y, con.select_end_y), (con.win->portRect));
	Rect bottomright = cell_rect(con.size_x, MAX(con.select_start_y, con.select_end_y), (con.win->portRect));

	UnionRect(&topleft, &bottomright, &topleft);
	InvalRect(&topleft);
}

void update_selection_end(void)
{
	static int last_mouse_cell_x = -1;
	static int last_mouse_cell_y = -1;

	int new_mouse_cell_x;
	int new_mouse_cell_y;

	Point new_mouse;

	GetMouse(&new_mouse);
	point_to_cell(new_mouse, &new_mouse_cell_x, &new_mouse_cell_y);

	// only damage the selection if the mouse has moved outside of the last cell
	if (last_mouse_cell_x != new_mouse_cell_x || last_mouse_cell_y != new_mouse_cell_y)
	{
		con.select_end_x = new_mouse_cell_x;
		con.select_end_y = new_mouse_cell_y;

		last_mouse_cell_x = new_mouse_cell_x;
		last_mouse_cell_y = new_mouse_cell_y;

		// damage the new selection
		damage_selection();
	}
}

inline void prev(int* x, int* y)
{
	if (*x == 0)
	{
		*x = con.size_x - 1;
		*y = (*y) - 1;
	}
	else
	{
		*x = (*x) - 1;
	}
}

inline void next(int* x, int* y)
{
	if (*x == con.size_x -1)
	{
		*x = 0;
		*y = (*y) + 1;
	}
	else
	{
		*x = (*x) + 1;
	}
}

void select_word(void)
{
	Point mouse;
	GetMouse(&mouse);

	VTermPos pos;
	point_to_cell(mouse, &pos.col, &pos.row);
	pos.col++;

	ScreenCell* vtsc = NULL;
	char c;

	// scan backwards from mouse click point
	while (!(pos.col == 0 && pos.row == 0))
	{
		prev(&pos.col, &pos.row);

		vtsc = vterm_screen_unsafe_get_cell(con.vts, pos);
		c = (char)vtsc->chars[0];
		if (c == '\0' || c == ' ')
		{
			next(&pos.col, &pos.row);
			break;
		}
	}

	con.select_start_x = pos.col;
	con.select_start_y = pos.row;

	// scan forwards from mouse click point
	point_to_cell(mouse, &pos.col, &pos.row);
	pos.col--;

	while (!(pos.col == con.size_x - 1 && pos.row == con.size_y -1))
	{
		next(&pos.col, &pos.row);

		vtsc = vterm_screen_unsafe_get_cell(con.vts, pos);
		c = (char)vtsc->chars[0];
		if (c == '\0' || c == ' ')
		{
			break;
		}
	}

	con.select_end_x = pos.col;
	con.select_end_y = pos.row;

	damage_selection();
}

// p is in window local coordinates
void mouse_click(Point p, int click)
{
	static Point last_click;
	static unsigned last_click_time = 0;

	con.mouse_state = click;

	if (con.mouse_mode == CLICK_SEND)
	{
		int row = p.v / con.cell_height;
		int col = p.h / con.cell_width;
		vterm_mouse_move(con.vterm, row, col, VTERM_MOD_NONE);
		vterm_mouse_button(con.vterm, 1, click, VTERM_MOD_NONE);
	}
	else if (con.mouse_mode == CLICK_SELECT)
	{
		if (click)
		{
			// damage the old selection so it gets wiped from the screen
			damage_selection();
			last_click = p;

			// if it's a double click
			if (TickCount() - last_click_time < GetDblTime())
			{
				select_word();
			}
			else
			{
				point_to_cell(p, &con.select_start_x, &con.select_start_y);
				point_to_cell(p, &con.select_end_x, &con.select_end_y);

				update_selection_end();
			}

			last_click_time = TickCount();
		}
		else
		{
			int a, b, c, d;
			point_to_cell(last_click, &a, &b);
			point_to_cell(p, &c, &d);

			// if in same cell, cancel the selection
			//if (a == c && b == d) clear_selection();

			//update_selection_end();
		}
	}
}

size_t get_selection(char** selection)
{
	if (con.select_start_x == -1 || con.select_start_y == -1)
	{
		*selection = NULL;
		return 0;
	}
	int a = con.select_start_x + con.select_start_y * con.size_x;
	int b = con.select_end_x + con.select_end_y * con.size_x;

	ssize_t len = MAX(a,b) - MIN(a,b) + 1;

	char* output = malloc(sizeof(char) * len);

	int start_row = MIN(con.select_start_y, con.select_end_y);
	int start_col = MIN(con.select_start_x, con.select_end_x);
	//int end_row = MAX(con.select_start_y, con.select_end_y);
	//int end_col = MAX(con.select_start_x, con.select_end_x);

	VTermPos pos = {.row = start_row, .col = start_col-1};
	ScreenCell* vtsc = NULL;

	for(int i = 0; i < len; i++)
	{
		next(&pos.col, &pos.row);

		vtsc = vterm_screen_unsafe_get_cell(con.vts, pos);
		output[i] = (char)vtsc->chars[0];
	}

	output[len-1] = '\0';

	*selection = output;

	return len;
}

// TODO: find a way to render this once and then just paste it on
inline void draw_resize_corner(void)
{
	// draw the grow icon in the bottom right corner, but not the scroll bars
	// yes, this is really awkward
	MacRegion bottom_right_corner = { 10, con.win->portRect};
	MacRegion* brc = &bottom_right_corner;
	MacRegion** old = con.win->clipRgn;

	bottom_right_corner.rgnBBox.top = bottom_right_corner.rgnBBox.bottom - 15;
	bottom_right_corner.rgnBBox.left = bottom_right_corner.rgnBBox.right - 15;

	con.win->clipRgn = &brc;
	DrawGrowIcon(con.win);
	con.win->clipRgn = old;
}

void draw_screen_color(Rect* r)
{
	// get the intersection of our console region and the update region
	//Rect bounds = (con.win->portRect);
	//SectRect(r, &bounds, r);

	// right now we only call this function to redraw the entire screen
	// so we don't need this (yet)
	//short minRow = (0 > (r->top - bounds.top) / con.cell_height) ? 0 : (r->top - bounds.top) / con.cell_height;
	//short maxRow = (con.size_y < (r->bottom - bounds.top + con.cell_height - 1) / con.cell_height) ? con.size_y : (r->bottom - bounds.top + con.cell_height - 1) / con.cell_height;

	//short minCol = (0 > (r->left - bounds.left) / con.cell_width) ? 0 : (r->left - bounds.left) / con.cell_width;
	//short maxCol = (con.size_x < (r->right - bounds.left + con.cell_width - 1) / con.cell_width) ? con.size_x : (r->right - bounds.left + con.cell_width - 1) / con.cell_width;

	// don't clobber font settings
	short save_font      = qd.thePort->txFont;
	short save_font_size = qd.thePort->txSize;
	short save_font_face = qd.thePort->txFace;
	short save_font_fg   = qd.thePort->fgColor;
	short save_font_bg   = qd.thePort->bkColor;

	short minRow = 0;
	short maxRow = con.size_y;
	short minCol = 0;
	short maxCol = con.size_x;

	TextFont(kFontIDMonaco);
	TextSize(prefs.font_size);
	TextFace(normal);
	BackColor(prefs.bg_color);
	ForeColor(prefs.fg_color);

	//EraseRect(&con.offscreen->portRect);

	TextMode(srcOr);

	short face = normal;
	Rect cr;

	ScreenCell* vtsc = NULL;
	VTermPos pos = {.row = 0, .col = 0};

	int i = 0;
	int select_start = -1;
	int select_end = -1;

	if (con.mouse_mode == CLICK_SELECT)
	{
		if (con.mouse_state) update_selection_end();

		if (con.select_start_x != -1)
		{
			int a = con.select_start_x + con.select_start_y * con.size_x;
			int b = con.select_end_x + con.select_end_y * con.size_x;

			if (a < b)
			{
				select_start = a;
				select_end = b;
			}
			else
			{
				select_start = b;
				select_end = a;
			}
		}
	}

	char c = 0;
	for(pos.row = minRow; pos.row < maxRow; pos.row++)
	{
		for (pos.col = minCol; pos.col < maxCol; pos.col++)
		{
			vtsc = vterm_screen_unsafe_get_cell(con.vts, pos);
			c = (char)vtsc->chars[0];

			if (vtsc->pen.reverse)
			{
				BackColor(idx2qd[vtsc->pen.fg.indexed.idx]);
				ForeColor(idx2qd[vtsc->pen.bg.indexed.idx]);
			}
			else
			{
				ForeColor(idx2qd[vtsc->pen.fg.indexed.idx]);
				BackColor(idx2qd[vtsc->pen.bg.indexed.idx]);
			}

			cr = cell_rect(pos.col, pos.row, *r);
			EraseRect(&cr);

			if (c == '\0' || c == ' ')
			{
				if (i < select_end && i >= select_start)
				{
					InvertRect(&cr);
				}
				i++;
				continue;
			}

			face = normal;
			if (vtsc->pen.bold) face |= (condense|bold);
			if (vtsc->pen.italic) face |= (condense|italic);
			if (vtsc->pen.underline) face |= underline;

			TextFace(face);
			draw_char(pos.col, pos.row, r, c);

			if (i < select_end && i >= select_start)
			{
				InvertRect(&cr);
			}
			i++;
		}
	}

	// do the cursor if needed
	if (con.cursor_state == 1 &&
		con.cursor_visible == 1)
	{
		Rect cursor = cell_rect(con.cursor_x, con.cursor_y, con.win->portRect);
		InvertRect(&cursor);
	}

	TextFont(save_font);
	TextSize(save_font_size);
	TextFace(save_font_face);
	qd.thePort->fgColor = save_font_fg;
	qd.thePort->bkColor = save_font_bg;

	draw_resize_corner();
}

void erase_row(int i)
{
	Rect left = cell_rect(0, i, (con.win->portRect));
	Rect right = cell_rect(con.size_x, i, (con.win->portRect));

	UnionRect(&left, &right, &left);
	EraseRect(&left);
}

void draw_screen_fast(Rect* r)
{
	// don't clobber font settings
	short save_font      = qd.thePort->txFont;
	short save_font_size = qd.thePort->txSize;
	short save_font_face = qd.thePort->txFace;
	short save_font_fg   = qd.thePort->fgColor;
	short save_font_bg   = qd.thePort->bkColor;

	TextFont(kFontIDMonaco);
	TextSize(prefs.font_size);
	TextFace(normal);
	qd.thePort->bkColor = whiteColor;
	qd.thePort->fgColor = blackColor;
	TextMode(srcOr); // or mode is faster for drawing black on white

	int select_start = -1;
	int select_end = -1;
	int i = 0;

	if (con.mouse_mode == CLICK_SELECT)
	{
		if (con.mouse_state) update_selection_end();

		if (con.select_start_x != -1)
		{
			int a = con.select_start_x + con.select_start_y * con.size_x;
			int b = con.select_end_x + con.select_end_y * con.size_x;

			if (a < b)
			{
				select_start = a;
				select_end = b;
			}
			else
			{
				select_start = b;
				select_end = a;
			}
		}
	}

	ScreenCell* vtsc = NULL;
	VTermPos pos = {.row = 0, .col = 0};

	char row_text[con.size_x];
	char row_invert[con.size_x];
	Rect cr;

	int vertical_offset = r->top + con.cell_height - font_offset + 2;

	for(pos.row = 0; pos.row < con.size_y; pos.row++)
	{
		erase_row(pos.row);

		for (pos.col = 0; pos.col < con.size_x; pos.col++)
		{
			vtsc = vterm_screen_unsafe_get_cell(con.vts, pos);
			row_text[pos.col] = (char)vtsc->chars[0];
			if (row_text[pos.col] == '\0') row_text[pos.col] = ' ';
			row_invert[pos.col] = vtsc->pen.reverse ^ (i < select_end && i >= select_start);
			i++;
		}

		MoveTo(r->left + 2, vertical_offset);
		DrawText(row_text, 0, con.size_x);

		for (int i = 0; i < con.size_x; i++)
		{
			if (row_invert[i])
			{
				cr = cell_rect(i, pos.row, con.win->portRect);
				InvertRect(&cr);
			}
		}
		vertical_offset += con.cell_height;
	}

	// do the cursor if needed
	if (con.cursor_state && con.cursor_visible)
	{
		Rect cursor = cell_rect(con.cursor_x, con.cursor_y, con.win->portRect);
		InvertRect(&cursor);
	}

	TextFont(save_font);
	TextSize(save_font_size);
	TextFace(save_font_face);
	qd.thePort->fgColor = save_font_fg;
	qd.thePort->bkColor = save_font_bg;

	//draw_resize_corner();
}

void draw_screen(Rect* r)
{
	if (prefs.display_mode == FASTEST)
	{
		draw_screen_fast(r);
	}
	else
	{
		draw_screen_color(r);
	}
}

void ruler(Rect* r)
{
	char itoc[] = {'0','1','2','3','4','5','6','7','8','9'};

	for (int x = 0; x < con.size_x; x++)
		for (int y = 0; y < con.size_y; y++)
			draw_char(x, y, r, itoc[x%10]);
}

int is_printable(char c)
{
	if (c >= 32 && c <= 126) return 1; else return 0;
}

void print_int(int d)
{
	char itoc[] = {'0','1','2','3','4','5','6','7','8','9'};

	char buffer[12] = {0};
	int i = 10;
	int negative = 0;

	if (d == 0)
	{
		buffer[0] = '0';
		i = -1;
	}

	if (d < 0)
	{
		negative = 1;
		d *= -1;
	}

	for (; d > 0; i--)
	{
		buffer[i] = itoc[d % 10];
		d /= 10;
	}

	if (negative) buffer[i] = '-';

	print_string(buffer+i+1-negative);
}

void printf_i(const char* str, ...)
{
	va_list args;
	va_start(args, str);

	while (*str != '\0')
	{
		if (*str == '%')
		{
			str++;
			switch (*str)
			{
				case 'd':
					print_int(va_arg(args, int));
					break;
				case 's':
					print_string(va_arg(args, char*));
					break;
				case '\0':
					vterm_input_write(con.vterm, str-1, 1);
					break;
				default:
					va_arg(args, int); // ignore
					vterm_input_write(con.vterm, str-1, 2);
					break;
			}
		}
		else
		{
			vterm_input_write(con.vterm, str, 1);
		}

		str++;
	}

	va_end(args);
}

int bell(void* user)
{
	SysBeep(30);

	return 1;
}

int movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
	// if the cursor is dark, invalidate that location
	if (con.cursor_state == 1)
	{
		Rect inval = cell_rect(con.cursor_x, con.cursor_y, (con.win->portRect));
		InvalRect(&inval);
	}

	con.cursor_x = pos.col;
	con.cursor_y = pos.row;

	return 1;
}

int damage(VTermRect rect, void *user)
{
	Rect topleft = cell_rect(rect.start_col, rect.start_row, (con.win->portRect));
	Rect bottomright = cell_rect(rect.end_col, rect.end_row, (con.win->portRect));

	UnionRect(&topleft, &bottomright, &topleft);
	InvalRect(&topleft);

	return 1;
}

int settermprop(VTermProp prop, VTermValue *val, void *user)
{
	switch (prop)
	{
		case VTERM_PROP_TITLE: // string
			set_window_title(con.win, val->string);
			return 1;
		case VTERM_PROP_CURSORVISIBLE: // bool
			con.cursor_visible = val->boolean;
			return 1;
		case VTERM_PROP_MOUSE: // number
			// record whether or not the terminal wants mouse clicks
			con.mouse_mode = (val->number == VTERM_PROP_MOUSE_CLICK) ? CLICK_SEND : CLICK_SELECT;
			damage_selection();
			clear_selection();
			return 1;
		case VTERM_PROP_ALTSCREEN: // bool
		case VTERM_PROP_ICONNAME: // string
		case VTERM_PROP_REVERSE: //bool
		case VTERM_PROP_CURSORSHAPE: // number
		case VTERM_PROP_CURSORBLINK: // bool
		default:
			break;
	}

	return 0;
}

void output_callback(const char *s, size_t len, void *user)
{
	ssh_write((char*)s, len);
}

const VTermScreenCallbacks vtscrcb =
{
	.damage = damage,
	.moverect = NULL,
	.movecursor = movecursor,
	.settermprop = settermprop,
	.bell = bell,
	.resize = NULL,
	.sb_pushline = NULL,
	.sb_popline = NULL
};

void console_setup(void)
{
	// don't clobber font settings
	short save_font = qd.thePort->txFont;
	short save_font_size = qd.thePort->txSize;
	short save_font_face = qd.thePort->txFace;

	con.size_x = 80;
	con.size_y = 24;

	TextFont(kFontIDMonaco);
	TextSize(prefs.font_size);
	TextFace(normal);

	FontInfo fi = {0};
	GetFontInfo(&fi);

	con.cell_height = fi.ascent + fi.descent + fi.leading + 1;
	font_offset = fi.descent;
	con.cell_width = fi.widMax;

	TextFont(save_font);
	TextSize(save_font_size);
	TextFace(save_font_face);

	Rect initial_window_bounds = qd.screenBits.bounds;

	InsetRect(&initial_window_bounds, 20, 20);
	initial_window_bounds.top += 40;

	initial_window_bounds.bottom = initial_window_bounds.top + con.cell_height * con.size_y + 4;
	initial_window_bounds.right = initial_window_bounds.left + con.cell_width * con.size_x + 4;

	ConstStr255Param title = "\pssheven " SSHEVEN_VERSION;

	WindowPtr win = NewWindow(NULL, &initial_window_bounds, title, true, documentProc, (WindowPtr)-1, true, 0);

	Rect portRect = win->portRect;

	SetPort(win);
	EraseRect(&portRect);

	con.win = win;

	con.cursor_x = 0;
	con.cursor_y = 0;

	con.vterm = vterm_new(con.size_y, con.size_x);
	vterm_set_utf8(con.vterm, 0);
	VTermState* vtermstate = vterm_obtain_state(con.vterm);
	vterm_state_reset(vtermstate, 1);

	VTermColor fg = { .type = VTERM_COLOR_INDEXED };
	fg.indexed.idx = qd2idx(prefs.fg_color);

	VTermColor bg  = { .type = VTERM_COLOR_INDEXED };
	bg.indexed.idx = qd2idx(prefs.bg_color);

	vterm_state_set_default_colors(vtermstate, &fg, &bg);

	vterm_output_set_callback(con.vterm, output_callback, NULL);

	con.vts = vterm_obtain_screen(con.vterm);
	vterm_screen_reset(con.vts, 1);
	vterm_screen_set_callbacks(con.vts, &vtscrcb, NULL);

	setup_key_translation();
}

// TODO: make this update all the cells with the default colors
void update_console_colors(void)
{
	VTermState* vtermstate = vterm_obtain_state(con.vterm);

	VTermColor fg = { .type = VTERM_COLOR_INDEXED };
	fg.indexed.idx = qd2idx(prefs.fg_color);

	VTermColor bg  = { .type = VTERM_COLOR_INDEXED };
	bg.indexed.idx = qd2idx(prefs.bg_color);

	vterm_state_set_default_colors(vtermstate, &fg, &bg);

	InvalRect(&(con.win->portRect));
}

