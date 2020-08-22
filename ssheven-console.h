/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#pragma once

#include "ssheven.h"
#include <string.h>

void console_setup(void);

void draw_char(int x, int y, Rect* r, char c);
void draw_screen(Rect* r);

void bump_up_line();

int is_printable(char c);

void print_char(char c);
void print_string(const char* c);
void print_int(int d);

void printf_i(const char* c, ...);

void set_window_title(WindowPtr w, const char* c_name);

void ruler(Rect* r);

Rect cell_rect(int x, int y, Rect bounds);

void toggle_cursor(void);
void check_cursor(void);
