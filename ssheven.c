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
#define OT_CHECK(X) err = (X); if (err != noErr) { print_string("" #X " failed\n"); return; };
#define SSH_CHECK(X) rc = (X); if (rc != LIBSSH2_ERROR_NONE) { print_string("" #X " failed: "); print_string(libssh2_error_string(rc)); print_string("\n"); return;};

// sinful globals
struct ssheven_console con = { NULL, {0}, 0, 0 };
struct ssheven_ssh_connection ssh_con = { NULL, NULL, kOTInvalidEndpointRef, NULL, NULL };

enum { WAIT, READ, EXIT } read_thread_command = WAIT;
enum { UNITIALIZED, OPEN, CLEANUP, DONE } read_thread_state = UNITIALIZED;

char hostname[256] = {0};
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

// read from the channel and print to console
void ssh_read(void)
{
	int rc = libssh2_channel_read(ssh_con.channel, ssh_con.recv_buffer, SSHEVEN_BUFFER_SIZE);

	if (rc == 0) return;

	if (rc > 0)
	{
		for(int i = 0; i < rc; ++i) print_char(ssh_con.recv_buffer[i]);
	}
	else
	{
		print_string("channel read error: ");
		print_string(libssh2_error_string(rc));
		print_string("\n");
	}

	// TODO invalidate only the correct region
	InvalRect(&(con.win->portRect));
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

	read_thread_state = DONE;
}

void check_network_events(void)
{
	OSStatus err = noErr;

	// check if we have any new network events
	OTResult look_result = OTLook(ssh_con.endpoint);

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

	return;
}

void event_loop(void)
{
	int exit_event_loop = 0;
	EventRecord event;
	WindowPtr eventWin;

	// maximum length of time to sleep (in ticks)
	// GetCaretTime gets the number of ticks between caret on/off time
	long int sleep_time = GetCaretTime();

	do
	{
		// wait to get a GUI event
		while (!WaitNextEvent(everyEvent, &event, sleep_time, NULL))
		{
			// timed out without any GUI events

			// process any network events
			//check_network_events();

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
					if (read_thread_state == OPEN) libssh2_channel_write(ssh_con.channel, ssh_con.send_buffer, 1);
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
							//not allowing resize right now
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

void init_connection(char* hostname)
{
	int rc;

	// OT vars
	OSStatus err = noErr;
	TCall sndCall;
	DNSAddress hostDNSAddress;
	OSStatus result;

	print_string_i("opening and configuring endpoint... "); YieldToAnyThread();

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

	print_string_i("done.\n"); YieldToAnyThread();

	// set up address struct, do the DNS lookup, and connect
	OTMemzero(&sndCall, sizeof(TCall));

	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

	print_string_i("connecting endpoint... "); YieldToAnyThread();
	OT_CHECK(OTConnect(ssh_con.endpoint, &sndCall, nil));

	print_string_i("done.\n"); YieldToAnyThread();

	print_string_i("initializing SSH... "); YieldToAnyThread();
	// init libssh2
	SSH_CHECK(libssh2_init(0));

	print_string_i("done.\n"); YieldToAnyThread();

	print_string_i("opening SSH session... "); YieldToAnyThread();
	ssh_con.session = libssh2_session_init();
	if (ssh_con.session == 0)
	{
		print_string("failed to initialize SSH library\n");
		return;
	}
	print_string_i("done.\n"); YieldToAnyThread();

	print_string_i("beginning SSH session handshake... "); YieldToAnyThread();
	SSH_CHECK(libssh2_session_handshake(ssh_con.session, ssh_con.endpoint));

	print_string_i("done.\n"); YieldToAnyThread();

	read_thread_state = OPEN;

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

	SSH_CHECK(libssh2_channel_request_pty(ssh_con.channel, SSHEVEN_TERMINAL_TYPE));
	SSH_CHECK(libssh2_channel_shell(ssh_con.channel));
}

int intro_dialog(char* hostname, char* username, char* password)
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
	} while(item != 1 && item != 9);

	// copy the text out of the boxes
	GetDialogItemText((Handle)address_text_box, hostname);
	GetDialogItemText((Handle)username_text_box, username);
	GetDialogItemText((Handle)password_text_box, password);

	// clean it up
	CloseDialog(dlg);
	FlushEvents(everyEvent, -1);

	// if we hit cancel, 0
	if (item == 9) return 0; else return 1;
}

//enum { WAIT, READ, EXIT } read_thread_command = WAIT;
//enum { UNITIALIZED, OPEN, CLEANUP, DONE } read_thread_state = UNITIALIZED;

// TODO: threads
void* read_thread(void* arg)
{
	while (read_thread_command == WAIT) YieldToAnyThread();

	if (read_thread_command == EXIT)
	{
		return 0;
	}

	// connect and log in
	init_connection(hostname+1);
	YieldToAnyThread();

	print_string_i("authenticating... "); YieldToAnyThread();
	ssh_password_auth(username+1, password+1);
	print_string_i("done.\n"); YieldToAnyThread();

	ssh_setup_terminal();
	YieldToAnyThread();

	// if we failed, exit
	if (read_thread_state != OPEN) return 0;

	// loop as long until we've failed or are asked to EXIT
	while (read_thread_command == READ && read_thread_state == OPEN)
	{
		check_network_events();
		YieldToAnyThread();
	}

	// if we still have a connection, close it
	if (read_thread_state != DONE) end_connection();

	return 0;
}

int main(int argc, char** argv)
{
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

	if (!intro_dialog(hostname, username, password)) return 0;

	console_setup();

	char* logo = "   _____ _____ _    _\n"
		"  / ____/ ____| |  | |\n"
		" | (___| (___ | |__| | _____   _____ _ __\n"
		"  \\___ \\\\___ \\|  __  |/ _ \\ \\ / / _ \\ '_ \\\n"
		"  ____) |___) | |  | |  __/\\ V /  __/ | | |\n"
		" |_____/_____/|_|  |_|\\___| \\_/ \\___|_| |_|\n";

	print_string(logo);
	print_string("by cy384, version " SSHEVEN_VERSION "\n");

	BeginUpdate(con.win);
	draw_screen(&(con.win->portRect));
	EndUpdate(con.win);

	int ok = 1;

	if (InitOpenTransport() != noErr)
	{
		print_string("failed to initialize OT\n");
		ok = 0;
	}

	if (ok)
	{
		ssh_con.recv_buffer = OTAllocMem(SSHEVEN_BUFFER_SIZE);
		ssh_con.send_buffer = OTAllocMem(SSHEVEN_BUFFER_SIZE);

		if (ssh_con.recv_buffer == NULL || ssh_con.send_buffer == NULL)
		{
			print_string("failed to allocate network buffers\n");
			ok = 0;
		}
	}

	// create the network read/print thread
	read_thread_command = WAIT;
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

	// if we got the thread, tell it to begin operation
	if (ok) read_thread_command = READ;

	// procede into our main event loop
	event_loop();

	// tell the read thread to quit, then let it run to actually do so
	read_thread_command = EXIT;
	YieldToAnyThread();

	//OTCancelSynchronousCalls(ssh_con.endpoint, kOTCanceledErr);
	//YieldToThread(read_thread_id);
	//	err = DisposeThread(read_thread_id, (void*)&read_thread_result, 0);
	//err = DisposeThread(read_thread_id, NULL, 0);

	end_connection();

	if (ssh_con.recv_buffer != NULL) OTFreeMem(ssh_con.recv_buffer);
	if (ssh_con.send_buffer != NULL) OTFreeMem(ssh_con.send_buffer);

	CloseOpenTransport();
}
