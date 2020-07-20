/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#include "ssheven-console.h"


void draw_char(int x, int y, Rect* r, char c)
{
	TextFont(kFontIDMonaco);
	TextSize(9);
	TextFace(normal);

	int cell_height = 12;
	int cell_width  = CharWidth('M');

	MoveTo(r->left + x * cell_width + 2, r->top + ((y+1) * cell_height) - 2);
	DrawChar(c);
}

void draw_screen(Rect* r)
{
	EraseRect(r);
	for (int x = 0; x < 80; x++)
		for (int y = 0; y < 24; y++)
			draw_char(x, y, r, con.data[x][y]);
}

void ruler(Rect* r)
{
	char itoc[] = {'0','1','2','3','4','5','6','7','8','9'};

	for (int x = 0; x < 80; x++)
		for (int y = 0; y < 24; y++)
			draw_char(x, y, r, itoc[x%10]);
}

void bump_up_line()
{
	for (int y = 0; y < 23; y++)
	{
		for (int x = 0; x < 80; x++)
		{
			con.data[x][y] = con.data[x][y+1];
		}
	}

	for (int x = 0; x < 80; x++) con.data[x][23] = ' ';
}

int is_printable(char c)
{
	if (c >= 32 && c <= 126) return 1; else return 0;
}

void print_char(char c)
{
	// backspace
	if ('\b' == c)
	{
		// erase current location
		con.data[con.cursor_x][con.cursor_y] = ' ';

		// wrap back to the previous line if possible and necessary
		if (con.cursor_x == 0 && con.cursor_y != 0)
		{
			con.cursor_x = 79;
			con.cursor_y--;
		}
		// otherwise just move back a spot
		else if (con.cursor_x > 0)
		{
			con.cursor_x--;
		}

		return;
	}

	// got a bell, give em a system beep (value of 30 recommended by docs)
	if ('\a' == c) SysBeep(30);

	if ('\n' == c)
	{
		con.cursor_y++;
		con.cursor_x = 0;
	}

	if (is_printable(c))
	{
		con.data[con.cursor_x][con.cursor_y] = c;
		con.cursor_x++;
	}

	if (con.cursor_x == 80)
	{
		con.cursor_x = 0;
		con.cursor_y++;
	}

	if (con.cursor_y == 24)
	{
		bump_up_line();
		con.cursor_y = 23;
	}
}

void print_string_i(const char* c)
{
	print_string(c);
	// TODO invalidate only the correct region
	InvalRect(&(con.win->portRect));
}

void print_string(const char* c)
{
	for (int i = 0; i < strlen(c); i++)
	{
		print_char(c[i]);
	}
}

void set_window_title(WindowPtr w, const char* c_name)
{
	Str255 pascal_name;
	strncpy((char *) &pascal_name[1], c_name, 255);
	pascal_name[0] = strlen(c_name);

	SetWTitle(w, pascal_name);
}


void console_setup(void)
{
	TextFont(kFontIDMonaco);
	TextSize(9);
	TextFace(normal);

	int cell_height = 12;
	int cell_width = CharWidth('M');

	Rect initial_window_bounds = qd.screenBits.bounds;
	InsetRect(&initial_window_bounds, 20, 20);
	initial_window_bounds.top += 40;

	initial_window_bounds.bottom = initial_window_bounds.top + cell_height * 24 + 2;
	initial_window_bounds.right = initial_window_bounds.left + cell_width * 80 + 4;

	// limits on window size changes:
	// top = min vertical
	// bottom = max vertical
	// left = min horizontal
	// right = max horizontal
	//Rect window_limits = { .top = 100, .bottom = 200, .left = 100, .right = 200 };

	ConstStr255Param title = "\pssheven " SSHEVEN_VERSION;

	WindowPtr win = NewWindow(NULL, &initial_window_bounds, title, true, noGrowDocProc, (WindowPtr)-1, true, 0);

	Rect portRect = win->portRect;

	SetPort(win);
	EraseRect(&portRect);

	int exit_main_loop = 0;

	con.win = win;
	memset(con.data, ' ', sizeof(char) * 24*80);

	con.cursor_x = 0;
	con.cursor_y = 0;
}

