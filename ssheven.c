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

enum { WAIT, READ, EXIT } read_thread_command = WAIT;
enum { UNINTIALIZED, OPEN, CLEANUP, DONE } read_thread_state = UNINTIALIZED;

char hostname[512] = {0};
char username[256] = {0};
char password[256] = {0};

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
			if (item == 1) exit = 1;
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
		unsigned char c = 0;

		switch(event.what)
		{
			case updateEvt:
				eventWin = (WindowPtr)event.message;
				BeginUpdate(eventWin);
				draw_screen(&(eventWin->portRect));
				EndUpdate(eventWin);
				break;

			case keyDown:
			case autoKey: // autokey means we're repeating a held down key event
				c = event.message & charCodeMask;
				// if we have a key and command and it's not autorepeating
				if (c && (event.modifiers & cmdKey) && event.what != autoKey)
				{
					switch(c)
					{
						case 'q':
							exit_event_loop = 1;
							break;
						case 'v':
							ssh_paste();
							break;
						default:
							break;
					}
				}
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

	read_thread_state = OPEN;

	return 1;
}

int ssh_setup_terminal(void)
{
	int rc = 0;

	SSH_CHECK(libssh2_channel_request_pty_ex(ssh_con.channel, SSHEVEN_TERMINAL_TYPE, (strlen(SSHEVEN_TERMINAL_TYPE)), NULL, 0, con.size_x, con.size_y, 0, 0));
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
int password_dialog(void)
{
	int ret = 1;
	// n.b. dialog strings can't be longer than this, so no overflow risk
	//static char password[256];
	DialogPtr dlog;
	Handle itemH;
	short item;
	Rect box;
	DialogItemType type;

	dlog = GetNewDialog(DLOG_PASSWORD, 0, (WindowPtr) - 1);

	// draw default button indicator around the connect button
	GetDialogItem(dlog, 2, &type, &itemH, &box);
	SetDialogItem(dlog, 2, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

	do {
		ModalDialog(NewModalFilterUPP(TwoItemFilter), &item);
	} while (item != 1 && item != 6); // until OK or cancel

	if (6 == item) ret = 0;

	// read out of the hidden text box
	GetDialogItem(dlog, 5, &type, &itemH, &box);
	GetDialogItemText(itemH, (unsigned char*)password);

	DisposeDialog(dlog);

	return ret;
}

int key_dialog(void)
{
	// TODO: keys
	printf_i("key authentication not implemented yet\r\n");
	return 0;
}

int intro_dialog(char* hostname, char* username, char* password)
{
	// modal dialog setup
	TEInit();
	InitDialogs(NULL);
	DialogPtr dlg = GetNewDialog(DLOG_CONNECT, 0, (WindowPtr)-1);
	InitCursor();

	// select all text in dialog item 4 (the hostname one)
	SelectDialogItemText(dlg, 4, 0, 32767);

	DialogItemType type;
	Handle itemH;
	Rect box;

	// draw default button indicator around the connect button
	GetDialogItem(dlg, 2, &type, &itemH, &box);
	SetDialogItem(dlg, 2, type, (Handle)NewUserItemUPP(&ButtonFrameProc), &box);

	// get the handles for each of the text boxes
	ControlHandle address_text_box;
	GetDialogItem(dlg, 4, &type, &itemH, &box);
	address_text_box = (ControlHandle)itemH;

	ControlHandle port_text_box;
	GetDialogItem(dlg, 5, &type, &itemH, &box);
	port_text_box = (ControlHandle)itemH;

	ControlHandle username_text_box;
	GetDialogItem(dlg, 7, &type, &itemH, &box);
	username_text_box = (ControlHandle)itemH;

	ControlHandle password_radio;
	GetDialogItem(dlg, 9, &type, &itemH, &box);
	password_radio = (ControlHandle)itemH;
	SetControlValue(password_radio, 1);

	ControlHandle key_radio;
	GetDialogItem(dlg, 10, &type, &itemH, &box);
	key_radio = (ControlHandle)itemH;
	SetControlValue(key_radio, 0);

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
	GetDialogItemText((Handle)address_text_box, (unsigned char *)hostname);
	GetDialogItemText((Handle)username_text_box, (unsigned char *)username);

	// splice the port number onto the hostname (n.b. they're pascal strings)
	GetDialogItemText((Handle)port_text_box, (unsigned char *)hostname+hostname[0]+1);
	hostname[hostname[0]+1] = ':';

	// clean it up
	DisposeDialog(dlg);
	FlushEvents(everyEvent, -1);

	// if we hit cancel, 0
	if (item == 8) return 0;

	if (GetControlValue(password_radio) == 1)
	{
		return password_dialog();
	}
	else
	{
		return key_dialog();
	}
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

	// connect and log in
	ok = init_connection(hostname+1);
	YieldToAnyThread();

	if (ok)
	{
		printf_i("Authenticating... "); YieldToAnyThread();
		rc = libssh2_userauth_password(ssh_con.session, username+1, password+1);

		if (rc == LIBSSH2_ERROR_NONE)
		{
			printf_i("done.\r\n");
		}
		else
		{
			if (rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED) StopAlert(ALRT_PW_FAIL, nil);
			printf_i("failure: %s\r\n", libssh2_error_string(rc));
			ok = 0;
		}
	}

	// if we logged in, open and set up the tty
	if (ok)
	{
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

	// process incoming data until we've failed or are asked to EXIT
	while (read_thread_command == READ && read_thread_state == OPEN)
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

	BeginUpdate(con.win);
	draw_screen(&(con.win->portRect));
	EndUpdate(con.win);

	int ok = 1;

	if (!safety_checks()) return 0;

	BeginUpdate(con.win);
	draw_screen(&(con.win->portRect));
	EndUpdate(con.win);

	ok = intro_dialog(hostname, username, password);

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

	if (con.vterm != NULL) vterm_free(con.vterm);

	if (ssh_con.endpoint != kOTInvalidEndpointRef)
	{
		err = OTCancelSynchronousCalls(ssh_con.endpoint, kOTCanceledErr);
		CloseOpenTransport();
	}
}
