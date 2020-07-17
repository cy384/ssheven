/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

// retro68 stdio/console library
//#include <stdio.h>

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

// libssh2
#include <libssh2.h>

// functions to convert error and status codes to strings
#include "ssheven-debug.h"

// version string
#define SSHEVEN_VERSION "0.1.0"

// size for recv and send thread buffers
#define BUFFER_SIZE 4096

// terminal type to send over ssh, determines features etc.
// "vanilla" supports basically nothing, which is good for us here
#define TERMINAL_TYPE "vanilla"

// error checking convenience macros
#define OT_CHECK(X) err = (X); if (err != noErr) { print_string("" #X " failed\n"); return; };
#define SSH_CHECK(X) rc = (X); if (rc != LIBSSH2_ERROR_NONE) { print_string("" #X " failed: "); print_string(libssh2_error_string(rc)); print_string("\n"); return;};

// sinful globals
struct ssheven_console
{
	WindowPtr win;

	char data[80][24];

	int cursor_x;
	int cursor_y;
} con = { NULL, {0}, 0, 0 };

struct ssheven_ssh_connection
{
	LIBSSH2_CHANNEL* channel;
	LIBSSH2_SESSION* session;

	EndpointRef endpoint;

	char* recv_buffer;
	char* send_buffer;
} ssh_con = { NULL, NULL, kOTInvalidEndpointRef, NULL, NULL };

enum { wait, read, exit } read_thread_state = wait;

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

// event handler for a thread to yield when blocked
static pascal void yield_notifier(void* contextPtr, OTEventCode code, OTResult result, void* cookie)
{
	switch (code)
	{
		case kOTSyncIdleEvent:
			YieldToAnyThread();
			break;

		default:
			break;
	}
}

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
	EraseRect(&(con.win->portRect));
	for (int x = 0; x < 80; x++)
		for (int y = 0; y < 24; y++)
			draw_char(x, y, r, con.data[x][y]);
}

char itoc[] = {'0','1', '2','3','4','5','6','7','8','9'};

void ruler(Rect* r)
{
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

void print_string(const char* c)
{
	for (int i = 0; i < strlen(c); i++)
	{
		print_char(c[i]);
	}
}

#include <string.h>
void set_window_title(WindowPtr w, const char* c_name)
{
	Str255 pascal_name;
	strncpy((char *) &pascal_name[1], c_name, 255);
	pascal_name[0] = strlen(c_name);

	SetWTitle(w, pascal_name);
}

void ssh_read(void)
{
	// read from the channel
	int rc = libssh2_channel_read(ssh_con.channel, ssh_con.recv_buffer, BUFFER_SIZE);

	if (rc == 0) return;

	if (rc > 0)
	{
		for(int i = 0; i < rc; ++i) print_char(ssh_con.recv_buffer[i]);
		InvalRect(&(con.win->portRect));
		//print_string("\n");
	}
	else
	{
		print_string("channel read error: ");
		print_string(libssh2_error_string(rc));
		print_string("\n");
	}
}


void end_connection(void)
{
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
		OT_CHECK(OTSndOrderlyDisconnect(ssh_con.endpoint));

		// get and discard remaining data so we can finish closing the connection
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
				print_string("unexpected OTLook result while closing: ");
				print_string(OT_event_string(result));
				print_string("\n");
				break;
		}

		// release endpoint
		err = OTUnbind(ssh_con.endpoint);
		if (err != noErr) print_string("OTUnbind failed\n");

		err = OTCloseProvider(ssh_con.endpoint);
		if (err != noErr) print_string("OTCloseProvider failed\n");
	}
}

void event_loop(void)
{
	int exit_event_loop = 0;
	OTResult look_result = 0;
	OSStatus err = noErr;

	do
	{
		// system task yields to run drivers and other stuff!
		//SystemTask(); // don't need to call if we use waitnextevent
		//Idle();
		EventRecord event;
		WindowPtr eventWin;

/*
		while(!GetNextEvent(everyEvent, &event))
		{
			SystemTask();
			YieldToAnyThread();
			//Idle();
		}
*/

		// alternately we can use:
		long int ct = GetCaretTime(); // should probably make this a smaller number, but eh.

		// wait for some length of time to get an event
		// runs the loop every time we timeout waiting for a mac event
		while (!WaitNextEvent(everyEvent, &event, ct, NULL))
		{
			// check if we have any new network events
			look_result = OTLook(ssh_con.endpoint);

			switch (look_result)
			{
				case T_DATA:
				case T_EXDATA:
					// got data
					ssh_read();
					break;

				case T_RESET:
					// connection reset? close it/give up
					end_connection();
					break;

				case T_DISCONNECT:
					// got disconnected
					OTRcvDisconnect(ssh_con.endpoint, nil);
					end_connection();
					break;

				case T_ORDREL:
					// nice tcp disconnect requested by remote
					err = OTRcvOrderlyDisconnect(ssh_con.endpoint);
					if (err == noErr)
					{
						err = OTSndOrderlyDisconnect(ssh_con.endpoint);
					}
					end_connection();
					break;

				default:
					// something weird or irrelevant: ignore it
					break;
			}

			// let any other threads run before we wait for events again
			YieldToAnyThread();
		}

		// handle any mac gui events
		char c = 0;
		switch(event.what)
		{
			// TODO: don't redraw the whole screen, just do needed region
			case updateEvt:
				eventWin = (WindowPtr)event.message;
				BeginUpdate(eventWin);
				draw_screen(&(eventWin->portRect));
				EndUpdate(eventWin);
				break;

			case keyDown:
			case autoKey: // autokey means we're repeating a held down key event
				c = event.message & charCodeMask;
				if (c)
				{
					if ('\r' == c) c = '\n';
					ssh_con.send_buffer[0] = c;
					libssh2_channel_write(ssh_con.channel, ssh_con.send_buffer, 1);
				}

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
						{
							//don't allow resize right now
							break;
							/*long growResult = GrowWindow(eventWin, event.where, &window_limits);
							SizeWindow(eventWin, growResult & 0xFFFF, growResult >> 16, true);
							EraseRect(&(eventWin->portRect));
							InvalRect(&(eventWin->portRect));*/
						}
						break;

					case inGoAway:
						{
							if (TrackGoAway(eventWin, event.where))
								exit_event_loop = 1;
						}
						break;

					case inMenuBar:
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

void init_connection(char* hostname)
{
	int rc;

	// OT vars
	OSStatus err = noErr;
	TCall sndCall;
	DNSAddress hostDNSAddress;
	OSStatus result;

	// open TCP endpoint
	ssh_con.endpoint = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);

	if (err != noErr || ssh_con.endpoint == kOTInvalidEndpointRef)
	{
		print_string("failed to open OT TCP endpoint\n");
		return;
	}

	OT_CHECK(OTSetSynchronous(ssh_con.endpoint));
	OT_CHECK(OTSetNonBlocking(ssh_con.endpoint));
	OT_CHECK(OTUseSyncIdleEvents(ssh_con.endpoint, false));

	OT_CHECK(OTBind(ssh_con.endpoint, nil, nil));

	// set up address struct, do the DNS lookup, and connect
	OTMemzero(&sndCall, sizeof(TCall));

	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

	OT_CHECK(OTConnect(ssh_con.endpoint, &sndCall, nil));

	print_string("OT setup done\n");

	// init libssh2
	SSH_CHECK(libssh2_init(0));

	ssh_con.session = libssh2_session_init();
	if (ssh_con.session == 0)
	{
		print_string("failed to initialize SSH library\n");
		return;
	}

	SSH_CHECK(libssh2_session_handshake(ssh_con.session, ssh_con.endpoint));

	return;
}

void ssh_password_auth(char* username, char* password)
{
	OSStatus err = noErr;
	int rc = 1;

	SSH_CHECK(libssh2_userauth_password(ssh_con.session, username, password));
	ssh_con.channel = libssh2_channel_open_session(ssh_con.session);
}

void ssh_setup_terminal(void)
{
	int rc = 0;

	SSH_CHECK(libssh2_channel_request_pty(ssh_con.channel, TERMINAL_TYPE));
	SSH_CHECK(libssh2_channel_shell(ssh_con.channel));
}

// TODO: unused
void* read_thread(void* arg)
{
	while(1)
	{
		switch(read_thread_state)
		{
			case wait:
				break;

			case read:
				ssh_read();
				InvalRect(&(con.win->portRect));
				break;

			case exit:
				return 0;
				break;
		}

		YieldToAnyThread();
	}
}

void intro_dialog(char* hostname, char* username, char* password)
{

	// modal dialog setup
	TEInit();
	InitDialogs(NULL);
	DialogPtr dlg = GetNewDialog(128,0,(WindowPtr)-1);
	InitCursor();

	// select all text in dialog item 4 (the hostname+port one)
	SelectDialogItemText(dlg, 4, 0, 32767);

	DialogItemType type;
	Handle itemH;
	Rect box;

	// draw default button indicator around the connect button
	GetDialogItem(dlg, 2, &type, &itemH, &box);
	SetDialogItem(dlg, 2, type, (Handle) NewUserItemUPP(&ButtonFrameProc), &box);

	// get the handles for each of the text boxes
	ControlHandle address_text_box;
	GetDialogItem(dlg, 4, &type, &itemH, &box);
	address_text_box = (ControlHandle)itemH;

	ControlHandle username_text_box;
	GetDialogItem(dlg, 6, &type, &itemH, &box);
	username_text_box = (ControlHandle)itemH;

	ControlHandle password_text_box;
	GetDialogItem(dlg, 8, &type, &itemH, &box);
	password_text_box = (ControlHandle)itemH;

	// let the modalmanager do everything
	// stop when the connect button is hit
	short item;
	do {
		ModalDialog(NULL, &item);
	} while(item != 1);

	// copy the text out of the boxes
	GetDialogItemText((Handle)address_text_box, hostname);
	GetDialogItemText((Handle)username_text_box, username);
	GetDialogItemText((Handle)password_text_box, password);

	// clean it up
	CloseDialog(dlg);
	FlushEvents(everyEvent, -1);
}

int main(int argc, char** argv)
{
	char hostname[256] = {0};
	char username[256] = {0};
	char password[256] = {0};

	OSStatus err = noErr;

	// expands the application heap to its maximum requested size
	// supposedly good for performance
	// also required before creating threads!
	MaxApplZone();

	// general gui setup
	InitGraf(&qd.thePort);
	InitFonts();
	InitWindows();
	InitMenus();

	intro_dialog(hostname, username, password);

	console_setup();

	char* logo = "   _____ _____ _    _\n"
		"  / ____/ ____| |  | |\n"
		" | (___| (___ | |__| | _____   _____ _ __\n"
		"  \\___ \\\\___ \\|  __  |/ _ \\ \\ / / _ \\ '_ \\\n"
		"  ____) |___) | |  | |  __/\\ V /  __/ | | |\n"
		" |_____/_____/|_|  |_|\\___| \\_/ \\___|_| |_|\n";

	print_string(logo);
	print_string("by cy384, version " SSHEVEN_VERSION "\n");

	int ok = 1;

	if (InitOpenTransport() != noErr)
	{
		print_string("failed to initialize OT\n");
		ok = 0;
	}

	if (ok)
	{
		ssh_con.recv_buffer = OTAllocMem(BUFFER_SIZE);
		ssh_con.send_buffer = OTAllocMem(BUFFER_SIZE);

		if (ssh_con.recv_buffer == NULL || ssh_con.send_buffer == NULL)
		{
			print_string("failed to allocate network buffers\n");
			ok = 0;
		}
	}

/*
	read_thread_state = wait;
	int read_thread_result = 0;
	ThreadID read_thread_id = 0;
	if (ok)
	{
		err = NewThread(kCooperativeThread, read_thread, NULL, 0, kCreateIfNeeded, NULL, &read_thread_id);

		if (err < 0)
		{
			ok = 0;
			print_string("failed to create network read thread\n");
		}
	}
*/

	if (ok)
	{
		// those strings are pascal strings, so we skip the first char
		init_connection(hostname+1);

		ssh_password_auth(username+1, password+1);
		ssh_setup_terminal();

		read_thread_state = read;
	}

	event_loop();

/*
	read_thread_state = exit;
	OTCancelSynchronousCalls(ssh_con.endpoint, kOTCanceledErr);
	YieldToThread(read_thread_id);
//	err = DisposeThread(read_thread_id, (void*)&read_thread_result, 0);
	err = DisposeThread(read_thread_id, NULL, 0);
*/

	end_connection();

	if (ssh_con.recv_buffer != NULL) OTFreeMem(ssh_con.recv_buffer);
	if (ssh_con.send_buffer != NULL) OTFreeMem(ssh_con.send_buffer);

	CloseOpenTransport();
}
