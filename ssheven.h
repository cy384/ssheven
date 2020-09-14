/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#pragma once

// open transport
#include <OpenTransport.h>
#include <OpenTptInternet.h>

// mac os stuff
#include <Threads.h>
#include <MacMemory.h>
#include <Quickdraw.h>
#include <Fonts.h>
#include <Windows.h>
#include <Sound.h>
#include <Gestalt.h>
#include <Devices.h>
#include <Scrap.h>
#include <Controls.h>
#include <ControlDefinitions.h>

// libssh2
#include <libssh2.h>

// ssheven constants
#include "ssheven-constants.r"

// libvterm
#include <vterm.h>
#include <vterm_keycodes.h>

// sinful globals
struct ssheven_console
{
	WindowPtr win;

	int size_x;
	int size_y;

	int cursor_x;
	int cursor_y;

	int cell_height;
	int cell_width;

	int cursor_state;
	long int last_cursor_blink;
	int cursor_visible;

	VTerm* vterm;
	VTermScreen* vts;
};

extern struct ssheven_console con;

struct ssheven_ssh_connection
{
	LIBSSH2_CHANNEL* channel;
	LIBSSH2_SESSION* session;

	EndpointRef endpoint;

	char* recv_buffer;
	char* send_buffer;
};

extern struct ssheven_ssh_connection ssh_con;

extern char key_to_vterm[256];

void ssh_write(char* buf, size_t len);
