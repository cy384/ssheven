/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#include "ssheven.h"
#include "ssheven-console.h"

// functions to convert error and status codes to strings
#include "ssheven-debug.c"

// error checking convenience macros
#define OT_CHECK(X) err = (X); if (err != noErr) { printf_i("" #X " failed\r\n"); return 0; };
#define SSH_CHECK(X) rc = (X); if (rc != LIBSSH2_ERROR_NONE) { printf_i("" #X " failed: %s\r\n", libssh2_error_string(rc)); return 0;};

// sinful globals
struct ssheven_console con = { NULL, 0, 0, 0, 0, 0, 0, 0, 0, 1, NULL, NULL };
struct ssheven_ssh_connection ssh_con = { NULL, NULL, kOTInvalidEndpointRef, NULL, NULL };
struct preferences prefs;

enum { WAIT, READ, EXIT } read_thread_command = WAIT;
enum { UNINTIALIZED, OPEN, CLEANUP, DONE } read_thread_state = UNINTIALIZED;

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
			prefs.terminal_string = "vt100";
			break;
		case MONOCHROME:
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
	//int create_new = 0;
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
		i += snprintf(output_buffer+i, write_length-i, "%d\n%d\n%d\n%d\n", (int)prefs.auth_type, (int)prefs.display_mode, (int)prefs.fg_color, (int)prefs.bg_color);

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
	prefs.display_mode = COLOR;
	prefs.fg_color = blackColor;
	prefs.bg_color = whiteColor;

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

	e = FSRead(prefRefNum, &buffer_size, buffer);
	e = FSClose(prefRefNum);

	// check the version (first two numbers)
	int items_got = sscanf(buffer, "%d\n%d", &prefs.major_version, &prefs.minor_version);
	if (items_got != 2) return;

	// only load a prefs file if the saved version number matches ours
	if ((prefs.major_version == SSHEVEN_VERSION_MAJOR) && (prefs.minor_version == SSHEVEN_VERSION_MINOR))
	{
		prefs.loaded_from_file = 1;
		items_got = sscanf(buffer, "%d\n%d\n%d\n%d\n%d\n%d\n%255[^\n]\n%255[^\n]\n%255[^\n]\n%[^\n]\n%[^\n]", &prefs.major_version, &prefs.minor_version, (int*)&prefs.auth_type, (int*)&prefs.display_mode, &prefs.fg_color, &prefs.bg_color, prefs.hostname+1, prefs.username+1, prefs.port+1, prefs.privkey_path, prefs.pubkey_path);

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

// read from the channel and print to console
void ssh_read(void)
{
	ssize_t rc = libssh2_channel_read(ssh_con.channel, ssh_con.recv_buffer, SSHEVEN_BUFFER_SIZE);

	if (rc == 0) return;

	if (rc <= 0)
	{
		printf_i("channel read error: %s\r\n", libssh2_error_string(rc));
	}

	while (rc > 0)
	{
		rc -= vterm_input_write(con.vterm, ssh_con.recv_buffer, rc);
	}
}

void end_connection(void)
{
	read_thread_state = CLEANUP;

	OSStatus err = noErr;

	if (ssh_con.channel)
	{
		libssh2_channel_send_eof(ssh_con.channel);
		libssh2_channel_close(ssh_con.channel);
		libssh2_channel_free(ssh_con.channel);
		libssh2_session_disconnect(ssh_con.session, "Normal Shutdown, Thank you for playing");
		libssh2_session_free(ssh_con.session);
	}

	libssh2_exit();

	if (ssh_con.endpoint != kOTInvalidEndpointRef)
	{
		// request to close the TCP connection
		OTSndOrderlyDisconnect(ssh_con.endpoint);

		// discard remaining data so we can finish closing the connection
		int rc = 1;
		OTFlags ot_flags;
		while (rc != kOTLookErr)
		{
			rc = OTRcv(ssh_con.endpoint, ssh_con.recv_buffer, 1, &ot_flags);
		}

		// finish closing the TCP connection
		OSStatus result = OTLook(ssh_con.endpoint);

		switch (result)
		{
			case T_DISCONNECT:
				OTRcvDisconnect(ssh_con.endpoint, nil);
				break;

			case T_ORDREL:
				err = OTRcvOrderlyDisconnect(ssh_con.endpoint);
				if (err == noErr)
				{
					err = OTSndOrderlyDisconnect(ssh_con.endpoint);
				}
				break;

			default:
				printf_i("Unexpected OTLook result while closing: %s\r\n", OT_event_string(result));
				break;
		}

		// release endpoint
		err = OTUnbind(ssh_con.endpoint);
		if (err != noErr) printf_i("OTUnbind failed.\r\n");

		err = OTCloseProvider(ssh_con.endpoint);
		if (err != noErr) printf_i("OTCloseProvider failed.\r\n");
	}

	read_thread_state = DONE;
}

int check_network_events(void)
{
	int ok = 1;
	OSStatus err = noErr;

	// check if we have any new network events
	OTResult look_result = OTLook(ssh_con.endpoint);

	switch (look_result)
	{
		case T_DATA:
		case T_EXDATA:
			// got data
			// we always try to read, so ignore this event
			break;

		case T_RESET:
			// connection reset? close it/give up
			end_connection();
			ok = 0;
			break;

		case T_DISCONNECT:
			// got disconnected
			OTRcvDisconnect(ssh_con.endpoint, nil);
			end_connection();
			ok = 0;
			break;

		case T_ORDREL:
			// nice tcp disconnect requested by remote
			err = OTRcvOrderlyDisconnect(ssh_con.endpoint);
			if (err == noErr)
			{
				err = OTSndOrderlyDisconnect(ssh_con.endpoint);
			}
			end_connection();
			ok = 0;
			break;

		default:
			// something weird or irrelevant: ignore it
			break;
	}

	return ok;
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

void ssh_write(char* buf, size_t len)
{
	if (read_thread_state == OPEN && read_thread_command != EXIT)
	{
		int r = libssh2_channel_write(ssh_con.channel, buf, len);

		if (r < 1)
		{
			printf_i("Failed to write to channel, closing connection.\r\n");
			read_thread_command = EXIT;
		}
	}
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

	// let the modalmanager do everything
	// stop on ok or cancel
	short item;
	do {
		ModalDialog(NULL, &item);
	} while(item != 1 && item != 9);

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
			if (item == 1) preferences_window();
			if (item == 3) exit = 1;
			break;

		case MENU_EDIT:
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
		int next_height = height - ((height - 2) % con.cell_height);
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

	// if we have a key and command and it's not autorepeating
	if (c && (event->modifiers & cmdKey) && event->what != autoKey)
	{
		switch (c)
		{
			case 'q':
				return 1;
				break;
			case 'v':
				ssh_paste();
				break;
			default:
				break;
		}
	}
	// if it's just a normal key
	// note that this is after translation, e.g. shift-f -> 'F', ctrl-a -> 0x01
	else if (c)
	{
		if (key_to_vterm[c] != VTERM_KEY_NONE)
		{
			vterm_keyboard_key(con.vterm, key_to_vterm[c], VTERM_MOD_NONE);
		}
		else
		{
			if ('\r' == c) c = '\n';
			ssh_con.send_buffer[0] = c;
			ssh_write(ssh_con.send_buffer, 1);
		}
	}

	return 0;
}

void event_loop(void)
{
	int exit_event_loop = 0;
	EventRecord event;
	WindowPtr eventWin;

	// maximum length of time to sleep (in ticks)
	// GetCaretTime gets the number of ticks between system caret on/off time
	long int sleep_time = GetCaretTime() / 4;

	do
	{
		// wait to get a GUI event
		while (!WaitNextEvent(everyEvent, &event, sleep_time, NULL))
		{
			// timed out without any GUI events
			// toggle our cursor if needed
			check_cursor();

			// then let any other threads run before we wait for events again
			YieldToAnyThread();
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
						break;
				}
				break;
		}

		YieldToAnyThread();
	} while (!exit_event_loop);
}

int init_connection(char* hostname)
{
	int rc;

	// OT vars
	OSStatus err = noErr;
	TCall sndCall;
	DNSAddress hostDNSAddress;

	printf_i("Opening and configuring endpoint... "); YieldToAnyThread();

	// open TCP endpoint
	ssh_con.endpoint = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);

	if (err != noErr || ssh_con.endpoint == kOTInvalidEndpointRef)
	{
		printf_i("failed to open Open Transport TCP endpoint.\r\n");
		return 0;
	}

	OT_CHECK(OTSetSynchronous(ssh_con.endpoint));
	OT_CHECK(OTSetBlocking(ssh_con.endpoint));
	OT_CHECK(OTUseSyncIdleEvents(ssh_con.endpoint, false));

	OT_CHECK(OTBind(ssh_con.endpoint, nil, nil));

	OT_CHECK(OTSetNonBlocking(ssh_con.endpoint));

	printf_i("done.\r\n"); YieldToAnyThread();

	// set up address struct, do the DNS lookup, and connect
	OTMemzero(&sndCall, sizeof(TCall));

	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

	printf_i("Connecting endpoint... "); YieldToAnyThread();
	OT_CHECK(OTConnect(ssh_con.endpoint, &sndCall, nil));

	printf_i("done.\r\n"); YieldToAnyThread();

	printf_i("Initializing SSH... "); YieldToAnyThread();
	// init libssh2
	SSH_CHECK(libssh2_init(0));

	printf_i("done.\r\n"); YieldToAnyThread();

	printf_i("Opening SSH session... "); YieldToAnyThread();
	ssh_con.session = libssh2_session_init();
	if (ssh_con.session == 0)
	{
		printf_i("failed to initialize SSH session.\r\n");
		return 0;
	}
	printf_i("done.\r\n"); YieldToAnyThread();

	long s = TickCount();
	printf_i("Beginning SSH session handshake... "); YieldToAnyThread();
	SSH_CHECK(libssh2_session_handshake(ssh_con.session, ssh_con.endpoint));

	printf_i("done. (%d ticks)\r\n", TickCount() - s); YieldToAnyThread();

	const char* banner = libssh2_session_banner_get(ssh_con.session);
	if (banner) printf_i("Server banner: %s\r\n", banner);

	read_thread_state = OPEN;

	return 1;
}

int ssh_setup_terminal(void)
{
	int rc = 0;

	SSH_CHECK(libssh2_channel_request_pty_ex(ssh_con.channel, prefs.terminal_string, (strlen(prefs.terminal_string)), NULL, 0, con.size_x, con.size_y, 0, 0));
	SSH_CHECK(libssh2_channel_shell(ssh_con.channel));

	return 1;
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
				break;

			case kLeftArrowCharCode:
			case kRightArrowCharCode:
			case kUpArrowCharCode:
			case kDownArrowCharCode:
			case kHomeCharCode:
			case kEndCharCode:
				return false; // don't handle them

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
		}
	}

	return false; // pass on other (non-keypress) events
}

// from the ATS password sample code
// 1 for ok, 0 for cancel
int password_dialog(int dialog_resource)
{
	int ret = 1;
	// n.b. dialog strings can't be longer than this, so no overflow risk
	//static char password[256];
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

// returns base64 sha256 hash of key as a malloc'd pascal string
char* host_hash(void)
{
	size_t length = 0;
	char* human_readable = malloc(64);
	memset(human_readable, 0, 64);
	const char* host_key_hash = NULL;

	host_key_hash = libssh2_hostkey_hash(ssh_con.session, LIBSSH2_HOSTKEY_HASH_SHA256);
	mbedtls_base64_encode((unsigned char*)human_readable+1, 64, &length, (unsigned const char*)host_key_hash, 32);

	human_readable[0] = (unsigned char)length;

	return human_readable;
}

char* known_hosts_full_path(int* found)
{
	int ok = 1;
	short foundVRefNum = 0;
	long foundDirID = 0;
	FSSpec known_hosts_file;
	*found = 0;

	OSType pref_type = 'SH7p';
	OSType creator_type = 'SSH7';

	// find the preferences folder on the system disk, create folder if needed
	OSErr e = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &foundVRefNum, &foundDirID);
	if (e != noErr) ok = 0;

	// make an FSSpec for the new file we want to make
	if (ok)
	{
		e = FSMakeFSSpec(foundVRefNum, foundDirID, "\pknown_hosts", &known_hosts_file);

		// if the file exists, we found it else make an empty one
		if (e == noErr) *found = 1;
		else if (e == noErr) e = FSpCreate(&known_hosts_file, creator_type, pref_type, smSystemScript);

		if (e != noErr) ok = 0;
	}

	Handle full_path_handle;
	int path_length;

	FSpPathFromLocation(&known_hosts_file, &path_length, &full_path_handle);
	char* full_path = malloc(path_length+1);
	strncpy(full_path, (char*)(*full_path_handle), path_length+1);
	DisposeHandle(full_path_handle);

	return full_path;
}

int known_hosts(void)
{
	int safe_to_connect = 1;
	int recognized_key = 0;
	int known_hosts_file_exists = 0;

	char* known_hosts_file_path = known_hosts_full_path(&known_hosts_file_exists);
	char* hash_string = NULL;

	size_t key_len = 0;
	int key_type = 0;
	const char* host_key = libssh2_session_hostkey(ssh_con.session, &key_len, &key_type);

	LIBSSH2_KNOWNHOSTS* known_hosts = libssh2_knownhost_init(ssh_con.session);

	if (known_hosts_file_exists)
	{
		// load known hosts file
		printf_i("Loading known hosts... ");

		int e = libssh2_knownhost_readfile(known_hosts, known_hosts_file_path, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
		if (e >= 0)
		{
			printf_i("got %d.\r\n", e);
		}
		else
		{
			printf_i("failed: %s\r\n", libssh2_error_string(e));
		}
	}
	else
	{
		printf_i("No known hosts file found.\r\n");
	}

	if (safe_to_connect)
	{
		// our hostname string includes the port, I guess that's okay?
		int e = libssh2_knownhost_check(known_hosts, prefs.hostname+1, host_key, key_len, key_type, NULL);
		switch (e)
		{
			case LIBSSH2_KNOWNHOST_CHECK_FAILURE:
				printf_i("Failed to check known hosts against server public key!\r\n");
				safe_to_connect = 0;
				break;
			case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
				printf_i("No matching host found.\r\n");
				break;
			case LIBSSH2_KNOWNHOST_CHECK_MATCH:
				printf_i("Host and key match found in known hosts.\r\n");
				recognized_key = 1;
				break;
			case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
				printf_i("Host found in known hosts but key doesn't match!\r\n");
				safe_to_connect = 0;
				break;
		}
	}

	hash_string = host_hash();
	//printf_i("Host key hash (SHA256): %s\r\n", hash_string+1);

	// ask the user to confirm if we're seeing a new host+key combo
	if (safe_to_connect && !recognized_key)
	{
		// ask the user if the key is OK
		DialogPtr dlg = GetNewDialog(DLOG_NEW_HOST, 0, (WindowPtr)-1);

		DialogItemType type;
		Handle itemH;
		Rect box;

		// draw default button indicator around the connect button
		GetDialogItem(dlg, 2, &type, &itemH, &box);
		SetDialogItem(dlg, 2, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

		// write the hash string into the dialog window
		ControlHandle hash_text_box;
		GetDialogItem(dlg, 4, &type, &itemH, &box);
		hash_text_box = (ControlHandle)itemH;
		SetDialogItemText((Handle)hash_text_box, (ConstStr255Param)hash_string);

		// let the modalmanager do everything
		// stop on reject or accept
		short item;
		do {
			ModalDialog(NULL, &item);
		} while(item != 1 && item != 5);

		// reject if user hit reject
		if (item == 1)
		{
			safe_to_connect = 0;
		}

		DisposeDialog(dlg);
		FlushEvents(everyEvent, -1);

		printf_i("Saving host and key... ");

		int save_type = (key_type == LIBSSH2_HOSTKEY_TYPE_RSA ? LIBSSH2_KNOWNHOST_KEY_SSHRSA : LIBSSH2_KNOWNHOST_KEY_SSHDSS);
		int e = libssh2_knownhost_addc(known_hosts, prefs.hostname+1, NULL, host_key, key_len, NULL, 0, LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW | save_type, NULL);

		if (e != 0) printf_i("failed to add to known hosts: %s\r\n", libssh2_error_string(e));
		else
		{
			e = libssh2_knownhost_writefile(known_hosts, known_hosts_file_path, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
			if (e != 0) printf_i("failed to save known hosts file: %s\r\n", libssh2_error_string(e));
			else printf_i("done.\r\n");
		}
	}

	free(hash_string);

	libssh2_knownhost_free(known_hosts);

	free(known_hosts_file_path);

	return safe_to_connect;
}

void* read_thread(void* arg)
{
	int ok = 1;
	int rc = LIBSSH2_ERROR_NONE;

	// yield until we're given a command
	while (read_thread_command == WAIT) YieldToAnyThread();

	if (read_thread_command == EXIT)
	{
		return 0;
	}

	// connect
	ok = init_connection(prefs.hostname+1);
	YieldToAnyThread();

	// check the server pub key vs. known hosts
	if (ok)
	{
		ok = known_hosts();
		if (!ok) printf_i("Rejected server public key!\r\n");
	}

	// actually log in
	if (ok)
	{
		printf_i("Authenticating... "); YieldToAnyThread();

		if (prefs.auth_type == USE_PASSWORD)
		{
			rc = libssh2_userauth_password(ssh_con.session, prefs.username+1, prefs.password+1);
		}
		else
		{
			rc = libssh2_userauth_publickey_fromfile_ex(ssh_con.session,
				prefs.username+1,
				prefs.username[0],
				prefs.pubkey_path,
				prefs.privkey_path,
				prefs.password+1);
		}

		if (rc == LIBSSH2_ERROR_NONE)
		{
			printf_i("done.\r\n");
		}
		else
		{
			printf_i("failed!\r\n");
			if (rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED && prefs.auth_type == USE_PASSWORD) StopAlert(ALRT_PW_FAIL, nil);
			else if (rc == LIBSSH2_ERROR_FILE) StopAlert(ALRT_FILE_FAIL, nil);
			else if (rc == LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED)
			{
				printf_i("Username/public key combination invalid!\r\n"); // TODO: have an alert for this
				if (prefs.pubkey_path[0] != '\0' || prefs.privkey_path[0] != '\0')
				{
					prefs.pubkey_path[0] = '\0';
					prefs.privkey_path[0] = '\0';
				}
			}
			else printf_i("unexpected failure: %s\r\n", libssh2_error_string(rc));
			ok = 0;
		}
	}

	save_prefs();

	// if we logged in, open and set up the tty
	if (ok)
	{
		libssh2_channel_handle_extended_data2(ssh_con.channel, LIBSSH2_CHANNEL_EXTENDED_DATA_MERGE);
		ssh_con.channel = libssh2_channel_open_session(ssh_con.session);
		ok = ssh_setup_terminal();
		YieldToAnyThread();
	}

	// if we failed, close everything and exit
	if (!ok || (read_thread_state != OPEN))
	{
		end_connection();
		return 0;
	}

	// if we connected, allow pasting
	void* menu = GetMenuHandle(MENU_EDIT);
	EnableItem(menu, 5);

	// read until failure, command to EXIT, or remote EOF
	while (read_thread_command == READ && read_thread_state == OPEN && libssh2_channel_eof(ssh_con.channel) == 0)
	{
		if (check_network_events()) ssh_read();
		YieldToAnyThread();
	}

	// if we still have a connection, close it
	if (read_thread_state != DONE) end_connection();

	// disallow pasting after connection is closed
	DisableItem(menu, 5);

	return 0;
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

	NumVersion* ot_version = (NumVersion*) &open_transport_new_version;

	printf_i("Detected Open Transport version: %d.%d.%d\r\n",
		(int)ot_version->majorRev,
		(int)((ot_version->minorAndBugRev & 0xF0) >> 4),
		(int)(ot_version->minorAndBugRev & 0x0F));

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
			if (cpu_type <= gestalt68030) cpu_slow = 1;
		}
	}
	else
	{
		if (cpu_type <= gestaltCPU68010) cpu_bad = 1;
		if (cpu_type <= gestaltCPU68030) cpu_slow = 1;
	}

	// if we don't have at least a 68020, stop
	if (cpu_bad)
	{
		StopAlert(ALRT_CPU_BAD, nil);
		return 0;
	}

	// if we don't have at least a 68040, warn
	if (cpu_slow)
	{
		CautionAlert(ALRT_CPU_SLOW, nil);
	}

	return 1;
}

int main(int argc, char** argv)
{
	OSStatus err = noErr;

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
	DisableItem(menu, 4);
	DisableItem(menu, 5);
	DisableItem(menu, 6);
	DisableItem(menu, 7);
	DisableItem(menu, 9);

	DrawMenuBar();

	console_setup();

	char* logo = "   _____ _____ _    _\r\n"
		"  / ____/ ____| |  | |\r\n"
		" | (___| (___ | |__| | _____   _____ _ __\r\n"
		"  \\___ \\\\___ \\|  __  |/ _ \\ \\ / / _ \\ '_ \\\r\n"
		"  ____) |___) | |  | |  __/\\ V /  __/ | | |\r\n"
		" |_____/_____/|_|  |_|\\___| \\_/ \\___|_| |_|\r\n";

	printf_i(logo);
	printf_i("by cy384, version " SSHEVEN_VERSION "\r\n");

	#if defined(__ppc__)
	printf_i("Running in PPC mode.\r\n");
	#else
	printf_i("Running in 68k mode.\r\n");
	#endif

	if (prefs.loaded_from_file)
	{
		printf_i("Loaded preferences file.\r\n");
	}
	else
	{
		printf_i("Could not load from preferences file.\r\n");
	}

	BeginUpdate(con.win);
	draw_screen(&(con.win->portRect));
	EndUpdate(con.win);

	int ok = 1;

	if (!safety_checks()) return 0;

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

	// procede into our main event loop
	event_loop();

	// tell the read thread to finish, then let it run to actually do so
	read_thread_command = EXIT;

	if (read_thread_state != UNINTIALIZED)
	{
		while (read_thread_state != DONE)
		{
			BeginUpdate(con.win);
			draw_screen(&(con.win->portRect));
			EndUpdate(con.win);
			YieldToAnyThread();
		}
	}

	if (ssh_con.recv_buffer != NULL) OTFreeMem(ssh_con.recv_buffer);
	if (ssh_con.send_buffer != NULL) OTFreeMem(ssh_con.send_buffer);

	if (prefs.pubkey_path != NULL && prefs.pubkey_path[0] != '\0') free(prefs.pubkey_path);
	if (prefs.privkey_path != NULL && prefs.privkey_path[0] != '\0') free(prefs.privkey_path);

	if (con.vterm != NULL) vterm_free(con.vterm);

	if (ssh_con.endpoint != kOTInvalidEndpointRef)
	{
		err = OTCancelSynchronousCalls(ssh_con.endpoint, kOTCanceledErr);
		CloseOpenTransport();
	}
}
