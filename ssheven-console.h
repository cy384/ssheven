/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#pragma once

#include "MacTypes.h"

void console_setup(void);

void draw_screen(Rect* r);

void printf_i(const char* c, ...);

void check_cursor(void);

void mouse_click(Point p, int click);

void update_console_colors(void);

size_t get_selection(char** selection);

void clear_selection(void);
