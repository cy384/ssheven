/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#pragma once

#include <OpenTransport.h>
#include <OpenTptInternet.h>
#include <StandardFile.h>
#include <Folders.h>

#include <libssh2.h>

#include <vterm.h>
#include <vterm_keycodes.h>

#include "ssheven-constants.r"

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

	enum { CLICK_SEND, CLICK_SELECT } mouse_mode;

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

struct preferences
{
	int major_version;
	int minor_version;

	int loaded_from_file;

	// pascal strings
	char hostname[512]; // of the form: "hostname:portnumber", size is first only
	char username[256];
	char password[256];
	char port[256];

	// malloc'd c strings
	char* pubkey_path;
	char* privkey_path;

	const char* terminal_string;

	enum { USE_KEY, USE_PASSWORD } auth_type;

	enum { FASTEST, MONOCHROME, COLOR } display_mode;
	int fg_color;
	int bg_color;
};

extern struct preferences prefs;

extern char key_to_vterm[256];

enum THREAD_COMMAND { WAIT, READ, EXIT };
enum THREAD_STATE { UNINTIALIZED, OPEN, CLEANUP, DONE };

extern enum THREAD_COMMAND read_thread_command;
extern enum THREAD_STATE read_thread_state;

int save_prefs(void);
void set_window_title(WindowPtr w, const char* c_name);

OSErr FSpPathFromLocation(FSSpec* spec, int* length, Handle* fullPath);

pascal void ButtonFrameProc(DialogRef dlg, DialogItemIndex itemNo);
