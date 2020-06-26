/* 
 * ssheven
 * 
 * Copyright (c) 2020 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

// retro68 stdio/console library
#include <stdio.h>

// open transport
#include <OpenTransport.h>
#include <OpenTptInternet.h>

// mac os threads
#include <Threads.h>

// libssh2
#include <libssh2.h>

// functions to convert error and status codes to strings
#include "ssheven-debug.h"

#define SSHEVEN_VERSION "0.0.0"

// network buffer size
enum { buffer_size = 4096 };

// text input buffer size
enum { input_buffer_size = 128 };

// event handler to yield whenever we're blocked
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

void get_line(char* buffer)
{
	int i = 0;
	char c;

	while (i < input_buffer_size - 1)
	{
		c = getc(stdin);
		if (c != '\n') buffer[i++] = c; else break;
	}

	buffer[i] = '\0';
	return;
}

void do_ssh_connection(char* hostname, char* username, char* password, char* command)
{
	// libssh2 vars
	LIBSSH2_CHANNEL* channel;
	LIBSSH2_SESSION* session;
	int rc;

	// OT vars
	OSStatus err = noErr;
	char* buffer = NULL;
	EndpointRef endpoint = kOTInvalidEndpointRef;
	TCall sndCall;
	DNSAddress hostDNSAddress;
	OSStatus result;
	OTFlags ot_flags;

	// allocate buffer
	buffer = OTAllocMem(buffer_size);
	if (buffer == NULL)
	{
		printf("failed to allocate OT buffer\n");
		return;
	}

	// open TCP endpoint
	endpoint = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);
	if (err != noErr)
	{
		printf("failed to open TCP endpoint\n");
		if (buffer != NULL) OTFreeMem(buffer);
		return;
	}

	#define OT_CHECK(X) err = (X); if (err != noErr) { printf("" #X " failed: %d\n", err); goto OT_cleanup; };

	OT_CHECK(OTSetSynchronous(endpoint));
	OT_CHECK(OTSetBlocking(endpoint));
	OT_CHECK(OTInstallNotifier(endpoint, yield_notifier, nil));
	OT_CHECK(OTUseSyncIdleEvents(endpoint, true));
	OT_CHECK(OTBind(endpoint, nil, nil));

	// set up address struct, do the DNS lookup, and connect
	OTMemzero(&sndCall, sizeof(TCall));

	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

	OT_CHECK(OTConnect(endpoint, &sndCall, nil));

	printf("OT setup done, endpoint: %d, (should not be %d for libssh2,"
		" should not be %d for OT)\n", (int) endpoint,
		(int) LIBSSH2_INVALID_SOCKET, (int) kOTInvalidEndpointRef);

	#define SSH_CHECK(X) rc = (X); if (rc != LIBSSH2_ERROR_NONE) { printf("" #X "failed: %s\n", libssh2_error_string(rc)); goto libssh2_cleanup; };

	// init libssh2
	SSH_CHECK(libssh2_init(0));

	session = libssh2_session_init();
	if (session == 0)
	{
		printf("failed to open SSH session\n");
		goto libssh2_cleanup;
	}

	SSH_CHECK(libssh2_session_handshake(session, endpoint));

	SSH_CHECK(libssh2_userauth_password(session, username, password));

	channel = libssh2_channel_open_session(session);
	printf("channel open: %d\n", channel);

	SSH_CHECK(libssh2_channel_exec(channel, command));

	// read from the channel
	rc = libssh2_channel_read(channel, buffer, buffer_size);
	if (rc > 0)
	{
		printf("got %d bytes:\n", rc);
		for(int i = 0; i < rc; ++i) printf("%c", buffer[i]);
		printf("\n");
	}
	else
	{
		printf("channel read error: %s\n", libssh2_error_string(rc));
	}

	libssh2_cleanup:

	libssh2_channel_close(channel);
	libssh2_channel_free(channel);
	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
	libssh2_session_free(session);
	libssh2_exit();

	// request to close the TCP connection
	OT_CHECK(OTSndOrderlyDisconnect(endpoint));

	// get and discard remaining data so we can finish closing the connection
	rc = 1;
	while (rc != kOTLookErr)
	{
		rc = OTRcv(endpoint, buffer, 1, &ot_flags);
	}

	// finish closing the TCP connection
	result = OTLook(endpoint);
	switch (result)
	{
		case T_DISCONNECT:
			OTRcvDisconnect(endpoint, nil);
			break;

		default:
			printf("unexpected OTLook result while closing: %d\n", result);
			break;
	}

	OT_cleanup:

	// release endpoint
	err = OTUnbind(endpoint);
	if (err != noErr) printf("OTUnbind failed: %d\n", err);

	err = OTCloseProvider(endpoint);
	if (err != noErr) printf("OTCloseProvider failed: %d\n", err);

	// if we have a buffer, release it
	if (buffer != nil) OTFreeMem(buffer);

	return;
}

int main(int argc, char** argv)
{
	char hostname[input_buffer_size] = {0};
	char username[input_buffer_size] = {0};
	char password[input_buffer_size] = {0};
	char command[input_buffer_size] = {0};

	printf("WARNING: this is a prototype with a bad RNG and no host key checks,"
		" do not use over untrusted networks or with untrusted SSH servers!\n\n");

	printf("ssheven by cy384 version " SSHEVEN_VERSION "\n\n");

	printf("enter a host:port >"); fflush(stdout);
	get_line(hostname);

	printf("enter a username  >"); fflush(stdout);
	get_line(username);

	printf("enter a password  >"); fflush(stdout);
	get_line(password);

	printf("enter a command   >"); fflush(stdout);
	get_line(command);

	if (InitOpenTransport() != noErr)
	{
		printf("failed to initialize OT\n");
		return 0;
	}

	do_ssh_connection(hostname, username, password, command);

	CloseOpenTransport();

	printf("\n(enter to exit)\n");
	getchar();

	return 0;
}

