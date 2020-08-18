/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#include "ssheven-console.h"


void draw_char(int x, int y, Rect* r, char c)
{
	MoveTo(r->left + x * con.cell_width + 2, r->top + ((y+1) * con.cell_height) - 2);
	DrawChar(c);
}

}

void draw_screen(Rect* r)
{
	// get the intersection of our console region and the update region
	Rect bounds = (con.win->portRect);
	SectRect(r, &bounds, r);

	short minRow = (0 > (r->top - bounds.top) / con.cell_height) ? 0 : (r->top - bounds.top) / con.cell_height;
	short maxRow = (24 < (r->bottom - bounds.top + con.cell_height - 1) / con.cell_height) ? 24 : (r->bottom - bounds.top + con.cell_height - 1) / con.cell_height;

	short minCol = (0 > (r->left - bounds.left) / con.cell_width) ? 0 : (r->left - bounds.left) / con.cell_width;
	short maxCol = (80 < (r->right - bounds.left + con.cell_width - 1) / con.cell_width) ? 80 : (r->right - bounds.left + con.cell_width - 1) / con.cell_width;

	EraseRect(r);

	// don't clobber font settings
	short save_font = qd.thePort->txFont;
	short save_font_size = qd.thePort->txSize;
	short save_font_face = qd.thePort->txFace;

	TextFont(kFontIDMonaco);
	TextSize(9);
	TextFace(normal);

	for(int i = minRow; i < maxRow; i++)
	{
		for (int j = minCol; j < maxCol; j++)
			draw_char(j, i, r, con.data[j][i]);
	}

	TextFont(save_font);
	TextSize(save_font_size);
	TextFace(save_font_face);
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

	InvalRect(&(con.win->portRect));
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
		Rect inval = cell_rect(con.cursor_x, con.cursor_y, (con.win->portRect));
		InvalRect(&inval);

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
		Rect inval = cell_rect(con.cursor_x, con.cursor_y, (con.win->portRect));
		InvalRect(&inval);
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

void print_int(int d)
{
	char itoc[] = {'0','1','2','3','4','5','6','7','8','9'};

	char buffer[12] = {0};
	int i = 10;

	if (d == 0)
	{
		buffer[0] = '0';
		i = -1;
	}

	for (; d > 0; i--)
	{
		buffer[i] = itoc[d % 10];
		d /= 10;
	}

	print_string(buffer+i+1);
}

void print_string(const char* c)
{
	while (*c != '\0')
	{
		print_char(*c++);
	}
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
				default:
					va_arg(args, int); // ignore
					print_char('%');
					print_char(*str);
					break;
			}
		}
		else
		{
			print_char(*str);
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


void console_setup(void)
{
	// don't clobber font settings
	short save_font = qd.thePort->txFont;
	short save_font_size = qd.thePort->txSize;
	short save_font_face = qd.thePort->txFace;

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

	initial_window_bounds.bottom = initial_window_bounds.top + con.cell_height * 24 + 2;
	initial_window_bounds.right = initial_window_bounds.left + con.cell_width * 80 + 4;

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

Rect cell_rect(int x, int y, Rect bounds)
{
	Rect r = { (short) (bounds.top + y * con.cell_height), (short) (bounds.left + x * con.cell_width + 2),
		(short) (bounds.top + (y+1) * con.cell_height), (short) (bounds.left + (x+1) * con.cell_width + 2) };

	return r;
}
