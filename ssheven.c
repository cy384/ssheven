/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#include "ssheven.h"
#include "ssheven-console.h"
#include "ssheven-net.h"
#include "ssheven-debug.h"

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

#include <stdio.h>

// sinful globals
struct ssheven_console con = { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, CLICK_SELECT, NULL, NULL };
struct ssheven_ssh_connection ssh_con = { NULL, NULL, kOTInvalidEndpointRef, NULL, NULL };
struct preferences prefs;

enum THREAD_COMMAND read_thread_command = WAIT;
enum THREAD_STATE read_thread_state = UNINITIALIZED;

const uint8_t ascii_to_control_code[256] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 27, 28, 29, 30, 31, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

// maps virtual keycodes to (hopefully) ascii characters
// this will probably be weird for non-letter keypresses
uint8_t keycode_to_ascii[256] = {0};

void generate_key_mapping(void)
{
	// TODO this sucks
	// gets the currently loaded keymap resource
	// passes in the virtual keycode but without any previous state or modifiers
	// ignores the second byte if we're currently using a multibyte lang

	void* kchr = (void*)GetScriptManagerVariable(smKCHRCache);

	for (uint16_t i = 0; i < 256; i++)
	{
		keycode_to_ascii[i] = KeyTranslate(kchr, i, 0) & 0xff;
	}
}

void set_window_title(WindowPtr w, const char* c_name)
{
	Str255 pascal_name;
	strncpy((char *) &pascal_name[1], c_name, 254);
	pascal_name[0] = strlen(c_name);

	SetWTitle(w, pascal_name);
}

void set_terminal_string(void)
{
	/*
	 * terminal type to send over ssh, determines features etc. some good options:
	 * "vanilla" supports basically nothing
	 * "vt100" just the basics
	 * "xterm" everything
	 * "xterm-mono" everything except color
	 * "xterm-16color" classic 16 ANSI colors only
	 */
	switch (prefs.display_mode)
	{
		case FASTEST:
			prefs.terminal_string = "xterm-mono";
			break;
		case COLOR:
			prefs.terminal_string = "xterm-16color";
			break;
		default:
			prefs.terminal_string = SSHEVEN_DEFAULT_TERM_STRING;
			break;
	}
}

int save_prefs(void)
{
	int ok = 1;
	short foundVRefNum = 0;
	long foundDirID = 0;
	FSSpec pref_file;
	short prefRefNum = 0;

	OSType pref_type = 'SH7p';
	OSType creator_type = 'SSH7';

	// find the preferences folder on the system disk, create folder if needed
	OSErr e = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &foundVRefNum, &foundDirID);
	if (e != noErr) ok = 0;

	// make an FSSpec for the new file we want to make
	if (ok)
	{
		e = FSMakeFSSpec(foundVRefNum, foundDirID, PREFERENCES_FILENAME, &pref_file);

		// if the file exists, delete it
		if (e == noErr) FSpDelete(&pref_file);

		// and then make it
		e = FSpCreate(&pref_file, creator_type, pref_type, smSystemScript);
		if (e != noErr) ok = 0;
	}

	// open the file
	if (ok)
	{
		e = FSpOpenDF(&pref_file, fsRdWrPerm, &prefRefNum);
		if (e != noErr) ok = 0;
	}

	// write prefs to the file
	if (ok)
	{
		// TODO: choose buffer size more effectively
		size_t write_length = 8192;

		char* output_buffer = malloc(write_length);
		memset(output_buffer, 0, write_length);

		long int i = snprintf(output_buffer, write_length, "%d\n%d\n", prefs.major_version, prefs.minor_version);
		i += snprintf(output_buffer+i, write_length-i, "%d\n%d\n%d\n%d\n%d\n", (int)prefs.auth_type, (int)prefs.display_mode, (int)prefs.fg_color, (int)prefs.bg_color, (int)prefs.font_size);

		snprintf(output_buffer+i, prefs.hostname[0]+1, "%s", prefs.hostname+1); i += prefs.hostname[0];
		i += snprintf(output_buffer+i, write_length-i, "\n");

		snprintf(output_buffer+i, prefs.username[0]+1, "%s", prefs.username+1); i += prefs.username[0];
		i += snprintf(output_buffer+i, write_length-i, "\n");

		snprintf(output_buffer+i, prefs.port[0]+1, "%s", prefs.port+1); i += prefs.port[0];
		i += snprintf(output_buffer+i, write_length-i, "\n");

		if (prefs.privkey_path && prefs.privkey_path[0] != '\0')
		{
			i += snprintf(output_buffer+i, write_length-i, "%s\n%s\n", prefs.privkey_path, prefs.pubkey_path);
		}
		else
		{
			i += snprintf(output_buffer+i, write_length-i, "\n\n");
		}

		// tell it to write all bytes
		long int bytes = i;
		e = FSWrite(prefRefNum, &bytes, output_buffer);
		// FSWrite sets bytes to the actual number of bytes written

		if (e != noErr || (bytes != i)) ok = 0;
	}

	// close the file
	if (prefRefNum != 0)
	{
		e = FSClose(prefRefNum);
		if (e != noErr) ok = 0;
	}

	return ok;
}

// check if the main device is black and white
int detect_color_screen(void)
{
	return TestDeviceAttribute(GetMainDevice(), gdDevType);
}

void init_prefs(void)
{
	// initialize everything to a safe default
	prefs.major_version = SSHEVEN_VERSION_MAJOR;
	prefs.minor_version = SSHEVEN_VERSION_MINOR;

	memset(&(prefs.hostname), 0, 512);
	memset(&(prefs.username), 0, 256);
	memset(&(prefs.password), 0, 256);
	memset(&(prefs.port), 0, 256);

	// default port: 22
	prefs.port[0] = 2;
	prefs.port[1] = '2';
	prefs.port[2] = '2';

	prefs.pubkey_path = "";
	prefs.privkey_path = "";
	prefs.terminal_string = SSHEVEN_DEFAULT_TERM_STRING;
	prefs.auth_type = USE_PASSWORD;
	prefs.display_mode = detect_color_screen() ? COLOR : FASTEST;
	prefs.fg_color = blackColor;
	prefs.bg_color = whiteColor;
	prefs.font_size = 9;

	prefs.loaded_from_file = 0;
}

void load_prefs(void)
{
	// now try to load preferences from the file
	short foundVRefNum = 0;
	long foundDirID = 0;
	FSSpec pref_file;
	short prefRefNum = 0;

	// find the preferences folder on the system disk
	OSErr e = FindFolder(kOnSystemDisk, kPreferencesFolderType, kDontCreateFolder, &foundVRefNum, &foundDirID);
	if (e != noErr) return;

	// make an FSSpec for the preferences file location and check if it exists
	// TODO: if I just put PREFERENCES_FILENAME it doesn't work, wtf
	e = FSMakeFSSpec(foundVRefNum, foundDirID, "\pssheven Preferences", &pref_file);

	if (e == fnfErr) // file not found, nothing to load
	{
		return;
	}
	else if (e != noErr) return;

	e = FSpOpenDF(&pref_file, fsCurPerm, &prefRefNum);
	if (e != noErr) return;

	// actually read and parse the file
	long int buffer_size = 8192;
	char* buffer = NULL;
	buffer = malloc(buffer_size);
	prefs.privkey_path = malloc(2048);
	prefs.pubkey_path = malloc(2048);
	prefs.pubkey_path[0] = '\0';
	prefs.privkey_path[0] = '\0';

	prefs.hostname[0] = 0;
	prefs.username[0] = 0;
	prefs.port[0] = 0;

	e = FSRead(prefRefNum, &buffer_size, buffer);
	e = FSClose(prefRefNum);

	// check the version (first two numbers)
	int items_got = sscanf(buffer, "%d\n%d", &prefs.major_version, &prefs.minor_version);
	if (items_got != 2) return;

	// only load a prefs file if the saved version number matches ours
	if ((prefs.major_version == SSHEVEN_VERSION_MAJOR) && (prefs.minor_version == SSHEVEN_VERSION_MINOR))
	{
		prefs.loaded_from_file = 1;
		items_got = sscanf(buffer, "%d\n%d\n%d\n%d\n%d\n%d\n%d\n%255[^\n]\n%255[^\n]\n%255[^\n]\n%[^\n]\n%[^\n]", &prefs.major_version, &prefs.minor_version, (int*)&prefs.auth_type, (int*)&prefs.display_mode, &prefs.fg_color, &prefs.bg_color, &prefs.font_size, prefs.hostname+1, prefs.username+1, prefs.port+1, prefs.privkey_path, prefs.pubkey_path);

		// add the size for the pascal strings
		prefs.hostname[0] = (unsigned char)strlen(prefs.hostname+1);
		prefs.username[0] = (unsigned char)strlen(prefs.username+1);
		prefs.port[0] = (unsigned char)strlen(prefs.port+1);

		set_terminal_string();
	}
	else
	{
		prefs.major_version = SSHEVEN_VERSION_MAJOR;
		prefs.minor_version = SSHEVEN_VERSION_MINOR;
	}

	if (buffer) free(buffer);
}

// borrowed from Retro68 sample code
// draws the "default" indicator around a button
pascal void ButtonFrameProc(DialogRef dlg, DialogItemIndex itemNo)
{
	DialogItemType type;
	Handle itemH;
	Rect box;

	GetDialogItem(dlg, 1, &type, &itemH, &box);
	InsetRect(&box, -4, -4);
	PenSize(3, 3);
	FrameRoundRect(&box, 16, 16);
}

void display_about_box(void)
{
	DialogRef about = GetNewDialog(DLOG_ABOUT, 0, (WindowPtr)-1);

	UpdateDialog(about, about->visRgn);

	while (!Button()) YieldToAnyThread();
	while (Button()) YieldToAnyThread();

	FlushEvents(everyEvent, 0);
	DisposeWindow(about);
}

void ssh_paste(void)
{
	// GetScrap requires a handle, not a raw buffer
	// it will increase the size of the handle if needed
	Handle buf = NewHandle(256);
	int r = GetScrap(buf, 'TEXT', 0);

	if (r > 0)
	{
		ssh_write(*buf, r);
	}

	DisposeHandle(buf);
}

void ssh_copy(void)
{
	char* selection = NULL;
	size_t len = get_selection(&selection);
	if (selection == NULL || len == 0) return;

	OSErr e = ZeroScrap();
	if (e != noErr) printf_i("Failed to ZeroScrap!");

	e = PutScrap(len, 'TEXT', selection);
	if (e != noErr) printf_i("Failed to PutScrap!");
}

int qd_color_to_menu_item(int qd_color)
{
	switch (qd_color)
	{
		case blackColor: return 1;
		case redColor: return 2;
		case greenColor: return 3;
		case yellowColor: return 4;
		case blueColor: return 5;
		case magentaColor: return 6;
		case cyanColor: return 7;
		case whiteColor: return 8;
		default: return 1;
	}
}

int menu_item_to_qd_color(int menu_item)
{
	switch (menu_item)
	{
		case 1: return blackColor;
		case 2: return redColor;
		case 3: return greenColor;
		case 4: return yellowColor;
		case 5: return blueColor;
		case 6: return magentaColor;
		case 7: return cyanColor;
		case 8: return whiteColor;
		default: return 1;
	}
}

int font_size_to_menu_item(int font_size)
{
	switch (font_size)
	{
		case 9:  return 1;
		case 10: return 2;
		case 12: return 3;
		case 14: return 4;
		case 18: return 5;
		case 24: return 6;
		case 36: return 7;
		default: return 1;
	}
}

int menu_item_to_font_size(int menu_item)
{
	switch (menu_item)
	{
		case 1:  return 9;
		case 2:  return 10;
		case 3:  return 12;
		case 4:  return 14;
		case 5:  return 18;
		case 6:  return 24;
		case 7:  return 36;
		default: return 9;
	}
}

void preferences_window(void)
{
	// modal dialog setup
	TEInit();
	InitDialogs(NULL);
	DialogPtr dlg = GetNewDialog(DLOG_PREFERENCES, 0, (WindowPtr)-1);
	InitCursor();

	// select all text in dialog item 4 (the hostname one)
	//SelectDialogItemText(dlg, 4, 0, 32767);

	DialogItemType type;
	Handle itemH;
	Rect box;

	// draw default button indicator around the connect button
	GetDialogItem(dlg, 2, &type, &itemH, &box);
	SetDialogItem(dlg, 2, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

	// get the handles for each menu, set to current prefs value
	ControlHandle term_type_menu;
	GetDialogItem(dlg, 6, &type, &itemH, &box);
	term_type_menu = (ControlHandle)itemH;
	SetControlValue(term_type_menu, prefs.display_mode + 1);

	ControlHandle bg_color_menu;
	GetDialogItem(dlg, 7, &type, &itemH, &box);
	bg_color_menu = (ControlHandle)itemH;
	SetControlValue(bg_color_menu, qd_color_to_menu_item(prefs.bg_color));

	ControlHandle fg_color_menu;
	GetDialogItem(dlg, 8, &type, &itemH, &box);
	fg_color_menu = (ControlHandle)itemH;
	SetControlValue(fg_color_menu, qd_color_to_menu_item(prefs.fg_color));

	ControlHandle font_size_menu;
	GetDialogItem(dlg, 10, &type, &itemH, &box);
	font_size_menu = (ControlHandle)itemH;
	SetControlValue(font_size_menu, font_size_to_menu_item(prefs.font_size));

	// let the modalmanager do everything
	// stop on ok or cancel
	short item;
	do {
		ModalDialog(NULL, &item);
	} while(item != 1 && item != 11);

	// save if OK'd
	if (item == 1)
	{
		// read menu values into prefs
		prefs.display_mode = GetControlValue(term_type_menu) - 1;

		// TODO: don't save colors, make it take effect immediately
		int save_bg = prefs.bg_color;
		int save_fg = prefs.fg_color;

		prefs.bg_color = menu_item_to_qd_color(GetControlValue(bg_color_menu));
		prefs.fg_color = menu_item_to_qd_color(GetControlValue(fg_color_menu));
		int new_font_size = menu_item_to_font_size(GetControlValue(font_size_menu));

		// resize window if font size changed
		if (new_font_size != prefs.font_size)
		{
			prefs.font_size = new_font_size;
			font_size_change();
		}

		save_prefs();

		prefs.bg_color = save_bg;
		prefs.fg_color = save_fg;

		// TODO: make this actually fix all colors in vterm
		update_console_colors();
	}

	// clean it up
	DisposeDialog(dlg);
	FlushEvents(everyEvent, -1);
}

// returns 1 if quit selected, else 0
int process_menu_select(int32_t result)
{
	int exit = 0;
	int16_t menu = (result & 0xFFFF0000) >> 16;
	int16_t item = (result & 0x0000FFFF);
	Str255 name;

	switch (menu)
	{
		case MENU_APPLE:
			if (item == 1)
			{
				display_about_box();
			}
			else
			{
				GetMenuItemText(GetMenuHandle(menu), item, name);
				OpenDeskAcc(name);
			}
			break;

		case MENU_FILE:
			if (item == 1)
			{
				if (connect() == 0) disconnect();
			}
			if (item == 2) disconnect();
			if (item == 4) preferences_window();
			if (item == 6) exit = 1;
			break;

		case MENU_EDIT:
			if (item == 4) ssh_copy();
			if (item == 5) ssh_paste();
			break;

		default:
			break;
	}

	HiliteMenu(0);
	return exit;
}

void resize_con_window(WindowPtr eventWin, EventRecord event)
{
	clear_selection();

	// TODO: put this somewhere else
	// limits on window size
	// top = min vertical
	// bottom = max vertical
	// left = min horizontal
	// right = max horizontal
	Rect window_limits = { .top = con.cell_height*2 + 2,
		.bottom = con.cell_height*100 + 2,
		.left = con.cell_width*10 + 4,
		.right = con.cell_width*200 + 4 };

	long growResult = GrowWindow(eventWin, event.where, &window_limits);

	if (growResult != 0)
	{
		int height = growResult >> 16;
		int width = growResult & 0xFFFF;

		// 'snap' to a size that won't have extra pixels not in a cell
		int next_height = height - ((height - 4) % con.cell_height);
		int next_width = width - ((width - 4) % con.cell_width);

		SizeWindow(eventWin, next_width, next_height, true);
		EraseRect(&(con.win->portRect));
		InvalRect(&(con.win->portRect));

		con.size_x = (next_width - 4)/con.cell_width;
		con.size_y = (next_height - 2)/con.cell_height;

		vterm_set_size(con.vterm, con.size_y, con.size_x);
		libssh2_channel_request_pty_size(ssh_con.channel, con.size_x, con.size_y);
	}
}

int handle_keypress(EventRecord* event)
{
	unsigned char c = event->message & charCodeMask;

	// if we have a key and command, and it's not autorepeating
	if (c && event->what != autoKey && event->modifiers & cmdKey)
	{
		switch (c)
		{
			case 'k':
				if (read_thread_state == UNINITIALIZED || read_thread_state == DONE)
				{
					if (connect() == 0) disconnect();
				}
				break;
			case 'd':
				if (read_thread_state == OPEN) disconnect();
				break;
			case 'q':
				return 1;
				break;
			case 'v':
				ssh_paste();
				break;
			case 'c':
				ssh_copy();
				break;
			default:
				break;
		}
	}
	else if (c)
	{
		// get the unmodified version of the keypress
		uint8_t unmodified_key = keycode_to_ascii[(event->message & keyCodeMask)>>8];

		// if we have a control code for this key
		if (event->modifiers & controlKey && ascii_to_control_code[unmodified_key] != 255)
		{
			ssh_con.send_buffer[0] = ascii_to_control_code[unmodified_key];
			ssh_write(ssh_con.send_buffer, 1);
		}
		else
		{
			// if we have alt and the character would be printable ascii, use it
			if (event->modifiers & optionKey && c >= 32 && c <= 126)
			{
				ssh_con.send_buffer[0] = c;
				ssh_write(ssh_con.send_buffer, 1);
			}
			else
			{
				// otherwise manually send an escape if we have alt held
				if (event->modifiers & optionKey)
				{
					ssh_con.send_buffer[0] = '\e';
					ssh_write(ssh_con.send_buffer, 1);
				}

				if (key_to_vterm[c] != VTERM_KEY_NONE)
				{
					// doesn't seem like vterm does modifiers properly, so don't bother
					vterm_keyboard_key(con.vterm, key_to_vterm[c], VTERM_MOD_NONE);
				}
				else
				{
					ssh_con.send_buffer[0] = event->modifiers & optionKey ? unmodified_key : c;
					ssh_write(ssh_con.send_buffer, 1);
				}
			}
		}
	}

	return 0;
}

void event_loop(void)
{
	int exit_event_loop = 0;
	EventRecord event;
	WindowPtr eventWin;
	Point local_mouse_position;

	// maximum length of time to sleep (in ticks)
	// GetCaretTime gets the number of ticks between system caret on/off time
	long int sleep_time = GetCaretTime() / 4;

	do
	{
		// wait to get a GUI event
		while (!WaitNextEvent(everyEvent, &event, sleep_time, NULL))
		{
			YieldToAnyThread();
			check_cursor();

			BeginUpdate(con.win);
			draw_screen(&(con.win->portRect));
			EndUpdate(con.win);
		}

		// might need to toggle our cursor even if we got an event
		check_cursor();

		// handle any GUI events
		switch (event.what)
		{
			case updateEvt:
				eventWin = (WindowPtr)event.message;
				BeginUpdate(eventWin);
				draw_screen(&(eventWin->portRect));
				EndUpdate(eventWin);
				break;

			case keyDown:
			case autoKey: // autokey means we're repeating a held down key event
				exit_event_loop = handle_keypress(&event);
				break;

			case mouseDown:
				switch(FindWindow(event.where, &eventWin))
				{
					case inDrag:
						// allow the window to be dragged anywhere on any monitor
						// hmmm... which of these is better???
						DragWindow(eventWin, event.where, &(*(GetGrayRgn()))->rgnBBox);
						//	DragWindow(eventWin, event.where, &(*qd.thePort->visRgn)->rgnBBox);
						break;

					case inGrow:
						resize_con_window(eventWin, event);
						break;

					case inGoAway:
						{
							if (TrackGoAway(eventWin, event.where))
								exit_event_loop = 1;
						}
						break;

					case inMenuBar:
						exit_event_loop = process_menu_select(MenuSelect(event.where));
						break;

					case inSysWindow:
						// is this system6 relevant only???
						SystemClick(&event, eventWin);
						break;

					case inContent:
						GetMouse(&local_mouse_position);
						mouse_click(local_mouse_position, true);
						break;
				}
				break;
			case mouseUp:
				// only tell the console to lift the mouse if we clicked through it
				if (con.mouse_state)
				{
					GetMouse(&local_mouse_position);
					mouse_click(local_mouse_position, false);
				}
				break;
		}

		YieldToAnyThread();
	} while (!exit_event_loop);
}

// from the ATS password sample code
pascal Boolean TwoItemFilter(DialogPtr dlog, EventRecord *event, short *itemHit)
{
	DialogPtr evtDlog;
	short selStart, selEnd;
	long unsigned ticks;
	Handle itemH;
	DialogItemType type;
	Rect box;

	// TODO: this should be declared somewhere? include it?
	int kVisualDelay = 8;

	if (event->what == keyDown || event->what == autoKey)
	{
		char c = event->message & charCodeMask;
		switch (c)
		{
			case kEnterCharCode: // select the ok button
			case kReturnCharCode: // we have to manually blink it...
			case kLineFeedCharCode:
				GetDialogItem(dlog, 1, &type, &itemH, &box);
				HiliteControl((ControlHandle)(itemH), kControlButtonPart);
				Delay(kVisualDelay, &ticks);
				HiliteControl((ControlHandle)(itemH), 1);
				*itemHit = 1;
				return true;

			case kTabCharCode: // cancel out tab events
				event->what = nullEvent;
				return false;

			case kEscapeCharCode: // hit cancel on esc or cmd-period
			case '.':
				if ((event->modifiers & cmdKey) || c == kEscapeCharCode)
				{
					GetDialogItem(dlog, 6, &type, &itemH, &box);
					HiliteControl((ControlHandle)(itemH), kControlButtonPart);
					Delay(kVisualDelay, &ticks);
					HiliteControl((ControlHandle)(itemH), 6);
					*itemHit = 6;
					return true;
				}
				__attribute__ ((fallthrough)); // fall through in case of a plain '.'

			default: // TODO: this is dumb and assumes everything else is a valid text character
				selStart = (**((DialogPeek)dlog)->textH).selStart; // Get the selection in the visible item
				selEnd = (**((DialogPeek)dlog)->textH).selEnd;
				SelectDialogItemText(dlog, 5, selStart, selEnd); // Select text in invisible item
				DialogSelect(event, &evtDlog, itemHit); // Input key
				SelectDialogItemText(dlog, 4, selStart, selEnd); // Select same area in visible item
				if ((event->message & charCodeMask) != kBackspaceCharCode) // If it's not a backspace (backspace is the only key that can affect both the text and the selection- thus we need to process it in both fields, but not change it for the hidden field.
					event->message = 0xa5; // Replace with character to use (the bullet)
				DialogSelect(event, &evtDlog, itemHit); // Put in fake character
				return true;

			case kLeftArrowCharCode:
			case kRightArrowCharCode:
			case kUpArrowCharCode:
			case kDownArrowCharCode:
			case kHomeCharCode:
			case kEndCharCode:
				return false; // don't handle them
		}
	}

	return false; // pass on other (non-keypress) events
}

// from the ATS password sample code
// 1 for ok, 0 for cancel
int password_dialog(int dialog_resource)
{
	int ret = 1;

	DialogPtr dlog;
	Handle itemH;
	short item;
	Rect box;
	DialogItemType type;

	dlog = GetNewDialog(dialog_resource, 0, (WindowPtr) - 1);

	// draw default button indicator around the connect button
	GetDialogItem(dlog, 2, &type, &itemH, &box);
	SetDialogItem(dlog, 2, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

	do {
		ModalDialog(NewModalFilterUPP(TwoItemFilter), &item);
	} while (item != 1 && item != 6); // until OK or cancel

	if (6 == item) ret = 0;

	// read out of the hidden text box
	GetDialogItem(dlog, 5, &type, &itemH, &box);
	GetDialogItemText(itemH, (unsigned char*)prefs.password);
	prefs.auth_type = USE_PASSWORD;

	DisposeDialog(dlog);

	return ret;
}

// derived from More Files sample code
OSErr
FSpPathFromLocation(
FSSpec *spec,		/* The location we want a path for. */
int *length,		/* Length of the resulting path. */
Handle *fullPath)		/* Handle to path. */
{
	OSErr err;
	FSSpec tempSpec;
	CInfoPBRec pb;

	*fullPath = NULL;

	/* 
	* Make a copy of the input FSSpec that can be modified.
	*/
	BlockMoveData(spec, &tempSpec, sizeof(FSSpec));

	if (tempSpec.parID == fsRtParID) {
		/*
		* The object is a volume.  Add a colon to make it a full
		* pathname.  Allocate a handle for it and we are done.
		*/
		tempSpec.name[0] += 2;
		tempSpec.name[tempSpec.name[0] - 1] = ':';
		tempSpec.name[tempSpec.name[0]] = '\0';

		err = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
	} else {
		/* 
		* The object isn't a volume.  Is the object a file or a directory? 
		*/
		pb.dirInfo.ioNamePtr = tempSpec.name;
		pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
		pb.dirInfo.ioDrDirID = tempSpec.parID;
		pb.dirInfo.ioFDirIndex = 0;
		err = PBGetCatInfoSync(&pb);

		if ((err == noErr) || (err == fnfErr)) {
			/*
			* If the file doesn't currently exist we start over.  If the
			* directory exists everything will work just fine.  Otherwise we
			* will just fail later.  If the object is a directory, append a
			* colon so full pathname ends with colon.
			*/
			if (err == fnfErr) {
			BlockMoveData(spec, &tempSpec, sizeof(FSSpec));
			} else if ( (pb.hFileInfo.ioFlAttrib & ioDirMask) != 0 ) {
			tempSpec.name[0] += 1;
			tempSpec.name[tempSpec.name[0]] = ':';
			}

			/* 
			* Create a new Handle for the object - make it a C string
			*/
			tempSpec.name[0] += 1;
			tempSpec.name[tempSpec.name[0]] = '\0';
			err = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
			if (err == noErr) {
				/* 
				* Get the ancestor directory names - loop until we have an
				* error or find the root directory.
				*/
				pb.dirInfo.ioNamePtr = tempSpec.name;
				pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
				pb.dirInfo.ioDrParID = tempSpec.parID;
				do {
					pb.dirInfo.ioFDirIndex = -1;
					pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
					err = PBGetCatInfoSync(&pb);
					if (err == noErr) {
						/* 
						* Append colon to directory name and add
						* directory name to beginning of fullPath
						*/
						++tempSpec.name[0];
						tempSpec.name[tempSpec.name[0]] = ':';
							
						(void) Munger(*fullPath, 0, NULL, 0, &tempSpec.name[1],
						tempSpec.name[0]);
						err = MemError();
					}
				} while ( (err == noErr) && (pb.dirInfo.ioDrDirID != fsRtDirID) );
			}
		}
	}

	/*
	* On error Dispose the handle, set it to NULL & return the err.
	* Otherwise, set the length & return.
	*/
	if (err == noErr) {
	*length = GetHandleSize(*fullPath) - 1;
	} else {
	if ( *fullPath != NULL ) {
	DisposeHandle(*fullPath);
	}
	*fullPath = NULL;
	*length = 0;
	}

	return err;
}

int key_dialog(void)
{
	Handle full_path = NULL;
	int path_length = 0;

	// if we don't have a saved pubkey path, ask for one
	if (prefs.pubkey_path == NULL || prefs.pubkey_path[0] == '\0')
	{
		NoteAlert(ALRT_PUBKEY, nil);
		StandardFileReply pubkey;
		StandardGetFile(NULL, 0, NULL, &pubkey);
		FSpPathFromLocation(&pubkey.sfFile, &path_length, &full_path);
		prefs.pubkey_path = malloc(path_length+1);
		strncpy(prefs.pubkey_path, (char*)(*full_path), path_length+1);
		DisposeHandle(full_path);

		// if the user hit cancel, 0
		if (!pubkey.sfGood) return 0;
	}

	path_length = 0;
	full_path = NULL;

	// if we don't have a saved privkey path, ask for one
	if (prefs.privkey_path == NULL || prefs.privkey_path[0] == '\0')
	{
		NoteAlert(ALRT_PRIVKEY, nil);
		StandardFileReply privkey;
		StandardGetFile(NULL, 0, NULL, &privkey);
		FSpPathFromLocation(&privkey.sfFile, &path_length, &full_path);
		prefs.privkey_path = malloc(path_length+1);
		strncpy(prefs.privkey_path, (char*)(*full_path), path_length+1);
		DisposeHandle(full_path);

		// if the user hit cancel, 0
		if (!privkey.sfGood) return 0;
	}

	// get the key decryption password
	if (!password_dialog(DLOG_KEY_PASSWORD)) return 0;

	prefs.auth_type = USE_KEY;
	return 1;
}

int intro_dialog(void)
{
	// modal dialog setup
	TEInit();
	InitDialogs(NULL);
	DialogPtr dlg = GetNewDialog(DLOG_CONNECT, 0, (WindowPtr)-1);
	InitCursor();

	DialogItemType type;
	Handle itemH;
	Rect box;

	// draw default button indicator around the connect button
	GetDialogItem(dlg, 2, &type, &itemH, &box);
	SetDialogItem(dlg, 2, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

	// get the handles for each of the text boxes, and load preference data in
	ControlHandle address_text_box;
	GetDialogItem(dlg, 4, &type, &itemH, &box);
	address_text_box = (ControlHandle)itemH;
	SetDialogItemText((Handle)address_text_box, (ConstStr255Param)prefs.hostname);

	// select all text in hostname dialog item
	SelectDialogItemText(dlg, 4, 0, 32767);

	ControlHandle port_text_box;
	GetDialogItem(dlg, 5, &type, &itemH, &box);
	port_text_box = (ControlHandle)itemH;
	SetDialogItemText((Handle)port_text_box, (ConstStr255Param)prefs.port);

	ControlHandle username_text_box;
	GetDialogItem(dlg, 7, &type, &itemH, &box);
	username_text_box = (ControlHandle)itemH;
	SetDialogItemText((Handle)username_text_box, (ConstStr255Param)prefs.username);

	ControlHandle password_radio;
	GetDialogItem(dlg, 9, &type, &itemH, &box);
	password_radio = (ControlHandle)itemH;
	SetControlValue(password_radio, 0);

	ControlHandle key_radio;
	GetDialogItem(dlg, 10, &type, &itemH, &box);
	key_radio = (ControlHandle)itemH;
	SetControlValue(key_radio, 0);

	// recall last-used connection type
	if (prefs.auth_type == USE_PASSWORD)
	{
		SetControlValue(password_radio, 1);
	}
	else
	{
		SetControlValue(key_radio, 1);
	}

	// let the modalmanager do everything
	// stop when the connect button is hit
	short item;
	do {
		ModalDialog(NULL, &item);
		if (item == 9)
		{
			SetControlValue(key_radio, 0);
			SetControlValue(password_radio, 1);
		}
		else if (item == 10)
		{
			SetControlValue(key_radio, 1);
			SetControlValue(password_radio, 0);
		}
	} while(item != 1 && item != 8);

	// copy the text out of the boxes
	GetDialogItemText((Handle)address_text_box, (unsigned char *)prefs.hostname);
	GetDialogItemText((Handle)username_text_box, (unsigned char *)prefs.username);

	GetDialogItemText((Handle)port_text_box, (unsigned char *)prefs.hostname+prefs.hostname[0]+1);
	prefs.hostname[prefs.hostname[0]+1] = ':';

	char* port_start = prefs.hostname+prefs.hostname[0] + 2;
	prefs.port[0] = strlen(port_start);
	strncpy(prefs.port+1, port_start, 255);

	int use_password = GetControlValue(password_radio);

	// clean it up
	DisposeDialog(dlg);
	FlushEvents(everyEvent, -1);

	// if we hit cancel, 0
	if (item == 8) return 0;

	if (use_password)
	{
		return password_dialog(DLOG_PASSWORD);
	}
	else
	{
		return key_dialog();
	}
}

int safety_checks(void)
{
	OSStatus err = noErr;

	// check for thread manager
	long int thread_manager_gestalt = 0;
	err = Gestalt(gestaltThreadMgrAttr, &thread_manager_gestalt);

	// bit one is prescence of thread manager
	if (err != noErr || (thread_manager_gestalt & (1 << gestaltThreadMgrPresent)) == 0)
	{
		StopAlert(ALRT_TM, nil);
		printf_i("Thread Manager not available!\r\n");
		return 0;
	}

	// check for Open Transport

	// for some reason, the docs say you shouldn't check for OT via the gestalt
	// in an application, and should just try to init, but checking seems more
	// user-friendly, so...

	long int open_transport_any_version = 0;
	long int open_transport_new_version = 0;
	err = Gestalt(gestaltOpenTpt, &open_transport_any_version);

	if (err != noErr)
	{
		printf_i("Failed to check for Open Transport!\r\n");
		return 0;
	}

	err = Gestalt(gestaltOpenTptVersions, &open_transport_new_version);

	if (err != noErr)
	{
		printf_i("Failed to check for Open Transport!\r\n");
		return 0;
	}

	if (open_transport_any_version == 0 && open_transport_new_version == 0)
	{
		printf_i("Open Transport required but not found!\r\n");
		StopAlert(ALRT_OT, nil);
		return 0;
	}

	if (open_transport_any_version != 0 && open_transport_new_version == 0)
	{
		printf_i("Early version of Open Transport detected!");
		printf_i("  Attempting to continue anyway.\r\n");
	}

	/*NumVersion* ot_version = (NumVersion*) &open_transport_new_version;

	printf_i("Detected Open Transport version: %d.%d.%d\r\n",
		(int)ot_version->majorRev,
		(int)((ot_version->minorAndBugRev & 0xF0) >> 4),
		(int)(ot_version->minorAndBugRev & 0x0F));*/

	// check for CPU type and display warning if it's going to be too slow
	long int cpu_type = 0;
	int cpu_slow = 0;
	int cpu_bad = 0;

	err = Gestalt(gestaltNativeCPUtype, &cpu_type);

	if (err != noErr || cpu_type == 0)
	{
		// earlier than 7.5, need to use other gestalt
		err = Gestalt(gestaltProcessorType, &cpu_type);
		if (err != noErr || cpu_type == 0)
		{
			cpu_slow = 1;
			printf_i("Failed to detect CPU type, continuing anyway.\r\n");
		}
		else
		{
			if (cpu_type <= gestalt68010) cpu_bad = 1;
			if (cpu_type <= gestalt68020) cpu_slow = 1;
		}
	}
	else
	{
		if (cpu_type <= gestaltCPU68010) cpu_bad = 1;
		if (cpu_type <= gestaltCPU68020) cpu_slow = 1;
	}

	// if we don't have at least a 68020, stop
	if (cpu_bad)
	{
		StopAlert(ALRT_CPU_BAD, nil);
		return 0;
	}

	// if we don't have at least a 68030, warn
	if (cpu_slow)
	{
		CautionAlert(ALRT_CPU_SLOW, nil);
	}

	return 1;
}

int connect(void)
{
	OSStatus err = noErr;
	int ok = 1;

	ok = safety_checks();

	// reset the console if we have any crap from earlier
	if (read_thread_state == DONE) reset_console();

	BeginUpdate(con.win);
	draw_screen(&(con.win->portRect));
	EndUpdate(con.win);

	ok = intro_dialog();

	if (!ok) printf_i("Cancelled, not connecting.\r\n");

	if (ok)
	{
		if (InitOpenTransport() != noErr)
		{
			printf_i("Failed to initialize Open Transport.\r\n");
			ok = 0;
		}
	}

	if (ok)
	{
		ssh_con.recv_buffer = OTAllocMem(SSHEVEN_BUFFER_SIZE);
		ssh_con.send_buffer = OTAllocMem(SSHEVEN_BUFFER_SIZE);

		if (ssh_con.recv_buffer == NULL || ssh_con.send_buffer == NULL)
		{
			printf_i("Failed to allocate network data buffers.\r\n");
			ok = 0;
		}
	}

	// create the network read/print thread
	read_thread_command = WAIT;
	ThreadID read_thread_id = 0;

	if (ok)
	{
		err = NewThread(kCooperativeThread, read_thread, NULL, 100000, kCreateIfNeeded, NULL, &read_thread_id);

		if (err < 0)
		{
			printf_i("Failed to create network read thread.\r\n");
			ok = 0;
		}
	}

	// if we got the thread, tell it to begin operation
	if (ok) read_thread_command = READ;

	// allow disconnecting if we're ok
	if (ok)
	{
		void* menu = GetMenuHandle(MENU_FILE);
		DisableItem(menu, 1);
		EnableItem(menu, 2);
	}

	return ok;
}

void disconnect(void)
{
	// tell the read thread to finish, then let it run to actually do so
	read_thread_command = EXIT;

	if (read_thread_state != UNINITIALIZED)
	{
		while (read_thread_state != DONE)
		{
			BeginUpdate(con.win);
			draw_screen(&(con.win->portRect));
			EndUpdate(con.win);
			YieldToAnyThread();
		}
	}

	if (ssh_con.recv_buffer != NULL)
	{
		OTFreeMem(ssh_con.recv_buffer);
		ssh_con.recv_buffer = NULL;
	}
	if (ssh_con.send_buffer != NULL)
	{
		OTFreeMem(ssh_con.send_buffer);
		ssh_con.send_buffer = NULL;
	}

	if (ssh_con.endpoint != kOTInvalidEndpointRef)
	{
		OTCancelSynchronousCalls(ssh_con.endpoint, kOTCanceledErr);
		CloseOpenTransport();
		ssh_con.endpoint = kOTInvalidEndpointRef;
	}

	// allow connecting if we're disconnected
	void* menu = GetMenuHandle(MENU_FILE);
	EnableItem(menu, 1);
	DisableItem(menu, 2);
}

int main(int argc, char** argv)
{
	// OSStatus err = noErr;

	// expands the application heap to its maximum requested size
	// supposedly good for performance
	// also required before creating threads!
	MaxApplZone();

	// "Call the MoreMasters procedure several times at the beginning of your program"
	MoreMasters();
	MoreMasters();

	// set default preferences, then load from preferences file if possible
	init_prefs();
	load_prefs();

	// general gui setup
	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();

	void* menu = GetNewMBar(MBAR_SSHEVEN);
	SetMenuBar(menu);
	AppendResMenu(GetMenuHandle(MENU_APPLE), 'DRVR');

	// disable stuff in edit menu until we implement it
	menu = GetMenuHandle(MENU_EDIT);
	DisableItem(menu, 1);
	DisableItem(menu, 3);
	//DisableItem(menu, 4);
	DisableItem(menu, 5);
	DisableItem(menu, 6);
	DisableItem(menu, 7);
	DisableItem(menu, 9);

	// disable connect and disconnect
	menu = GetMenuHandle(MENU_FILE);
	DisableItem(menu, 1);
	DisableItem(menu, 2);

	DrawMenuBar();

	generate_key_mapping();

	console_setup();

	char* logo = "   _____ _____ _    _\r\n"
		"  / ____/ ____| |  | |\r\n"
		" | (___| (___ | |__| | _____   _____ _ __\r\n"
		"  \\___ \\\\___ \\|  __  |/ _ \\ \\ / / _ \\ '_ \\\r\n"
		"  ____) |___) | |  | |  __/\\ V /  __/ | | |\r\n"
		" |_____/_____/|_|  |_|\\___| \\_/ \\___|_| |_|\r\n";

	printf_i(logo);
	printf_i("by cy384, version " SSHEVEN_VERSION ", running in ");

	#if defined(__ppc__)
	printf_i("PPC mode.\r\n");
	#else
	printf_i("68k mode.\r\n");
	#endif

	BeginUpdate(con.win);
	draw_screen(&(con.win->portRect));
	EndUpdate(con.win);

	int ok = connect();

	if (!ok) disconnect();

	event_loop();

	if (read_thread_command != EXIT) disconnect();

	if (con.vterm != NULL) vterm_free(con.vterm);

	if (prefs.pubkey_path != NULL && prefs.pubkey_path[0] != '\0') free(prefs.pubkey_path);
	if (prefs.privkey_path != NULL && prefs.privkey_path[0] != '\0') free(prefs.privkey_path);
}
