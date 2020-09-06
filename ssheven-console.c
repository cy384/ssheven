/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#include "ssheven-console.h"

char key_to_vterm[256] = { VTERM_KEY_NONE };

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

inline void draw_char(int x, int y, Rect* r, char c)
{
	MoveTo(r->left + x * con.cell_width + 2, r->top + ((y+1) * con.cell_height) - 2);
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
	long int now = TickCount();
	if ((now - con.last_cursor_blink) > GetCaretTime())
	{
		toggle_cursor();
		con.last_cursor_blink = now;
	}
}

// closely inspired by the retro68 console library
void draw_screen(Rect* r)
{
	// get the intersection of our console region and the update region
	Rect bounds = (con.win->portRect);
	SectRect(r, &bounds, r);

	short minRow = (0 > (r->top - bounds.top) / con.cell_height) ? 0 : (r->top - bounds.top) / con.cell_height;
	short maxRow = (con.size_y < (r->bottom - bounds.top + con.cell_height - 1) / con.cell_height) ? con.size_y : (r->bottom - bounds.top + con.cell_height - 1) / con.cell_height;

	short minCol = (0 > (r->left - bounds.left) / con.cell_width) ? 0 : (r->left - bounds.left) / con.cell_width;
	short maxCol = (con.size_x < (r->right - bounds.left + con.cell_width - 1) / con.cell_width) ? con.size_x : (r->right - bounds.left + con.cell_width - 1) / con.cell_width;

	EraseRect(r);

	// don't clobber font settings
	short save_font = qd.thePort->txFont;
	short save_font_size = qd.thePort->txSize;
	short save_font_face = qd.thePort->txFace;

	TextFont(kFontIDMonaco);
	TextSize(9);
	TextFace(normal);

	VTermScreenCell vtsc;
	short face = normal;
	char c;
	Rect cr;

	for(int i = minRow; i < maxRow; i++)
	{
		for (int j = minCol; j < maxCol; j++)
		{
			vterm_screen_get_cell(con.vts, (VTermPos){.row = i, .col = j}, &vtsc);
			c = (char)vtsc.chars[0];
			face = normal;
			if (vtsc.attrs.bold) face |= (condense|bold);
			if (vtsc.attrs.italic) face |= (condense|italic);
			if (vtsc.attrs.underline) face |= underline;

			if (face != normal) TextFace(face);
			draw_char(j, i, r, c);
			if (face != normal) TextFace(normal);

			if (vtsc.attrs.reverse)
			{
				cr = cell_rect(j,i,con.win->portRect);
				InvertRect(&cr);
			}
		}
	}

	// do the cursor if needed
	if (con.cursor_state == 1 &&
		con.cursor_y >= minRow &&
		con.cursor_y <= maxRow &&
		con.cursor_x >= minCol &&
		con.cursor_x <= maxCol)
	{
		Rect cursor = cell_rect(con.cursor_x, con.cursor_y, con.win->portRect);
		InvertRect(&cursor);
	}

	TextFont(save_font);
	TextSize(save_font_size);
	TextFace(save_font_face);

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

void print_string(const char* c)
{
	vterm_input_write(con.vterm, c, strlen(c));
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

void set_window_title(WindowPtr w, const char* c_name)
{
	Str255 pascal_name;
	strncpy((char *) &pascal_name[1], c_name, 255);
	pascal_name[0] = strlen(c_name);

	SetWTitle(w, pascal_name);
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

Rect cell_rect(int x, int y, Rect bounds)
{
	Rect r = { (short) (bounds.top + y * con.cell_height), (short) (bounds.left + x * con.cell_width + 2),
		(short) (bounds.top + (y+1) * con.cell_height), (short) (bounds.left + (x+1) * con.cell_width + 2) };

	return r;
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
		// TODO: handle more of these, particularly cursor visible and blink
		case VTERM_PROP_ICONNAME: // string
		case VTERM_PROP_REVERSE: //bool
		case VTERM_PROP_CURSORSHAPE: // number
		case VTERM_PROP_MOUSE: // number
		case VTERM_PROP_CURSORVISIBLE: // bool
		case VTERM_PROP_CURSORBLINK: // bool
		case VTERM_PROP_ALTSCREEN: // bool
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
	TextSize(9);
	TextFace(normal);

	con.cell_height = 12;
	con.cell_width = CharWidth('M');

	TextFont(save_font);
	TextSize(save_font_size);
	TextFace(save_font_face);

	Rect initial_window_bounds = qd.screenBits.bounds;
	InsetRect(&initial_window_bounds, 20, 20);
	initial_window_bounds.top += 40;

	initial_window_bounds.bottom = initial_window_bounds.top + con.cell_height * con.size_y + 2;
	initial_window_bounds.right = initial_window_bounds.left + con.cell_width * con.size_x + 4;

	ConstStr255Param title = "\pssheven " SSHEVEN_VERSION;

	WindowPtr win = NewWindow(NULL, &initial_window_bounds, title, true, documentProc, (WindowPtr)-1, true, 0);

	Rect portRect = win->portRect;

	SetPort(win);
	EraseRect(&portRect);

	int exit_main_loop = 0;

	con.win = win;

	con.cursor_x = 0;
	con.cursor_y = 0;

	con.vterm = vterm_new(con.size_y, con.size_x);
	vterm_set_utf8(con.vterm, 0);
	VTermState* vtermstate = vterm_obtain_state(con.vterm);
	vterm_state_reset(vtermstate, 1);

	vterm_output_set_callback(con.vterm, output_callback, NULL);

	con.vts = vterm_obtain_screen(con.vterm);
	vterm_screen_reset(con.vts, 1);
	vterm_screen_set_callbacks(con.vts, &vtscrcb, NULL);

	setup_key_translation();
}

