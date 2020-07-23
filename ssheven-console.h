/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#pragma once

#include "ssheven.h"
#include <string.h>

void draw_char(int x, int y, Rect* r, char c);
void draw_screen(Rect* r);

void ruler(Rect* r);

void bump_up_line();

int is_printable(char c);

void print_char(char c);
void print_string(const char* c);
void print_string_i(const char* c);
void print_int(int d);

void set_window_title(WindowPtr w, const char* c_name);

void console_setup(void);
