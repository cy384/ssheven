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

enum { buffer_size = 4096 };

void assertp(char* message, int b)
{
	if (!b) printf("assert fail: %s (%d)\n", message, b);
	getchar();
}

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

void do_ssh_connection(void)
{
	char* hostname = "cy384.com:22";
	char* command = "uname";
	char* username = "ssheven";
	char* password = "password";

	LIBSSH2_CHANNEL* channel;
	LIBSSH2_SESSION* session;

	int rc;

	// make and set up OT connection
	OSStatus err = noErr;
	Ptr buffer = nil;
	EndpointRef endpoint = kOTInvalidEndpointRef;
	TCall sndCall;
	DNSAddress hostDNSAddress;

	// allocate buffer
	buffer = OTAllocMem(buffer_size);
	if (buffer == nil)
	{
		err = kENOMEMErr;
	}

	// open TCP endpoint
	if (err == noErr)
	{
		endpoint = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);
	}

	// configure the endpoint
	// synchronous and blocking, and we yield until we get a result
	if (err == noErr)
	{
		OSStatus result;

		result = OTSetSynchronous(endpoint);
		assertp("OTSetSynchronous failed", result == noErr);

		result = OTSetBlocking(endpoint);
		assertp("OTSetBlocking failed", result == noErr);

		result = OTInstallNotifier(endpoint, yield_notifier, nil);
		assertp("OTInstallNotifier failed", result == noErr);

		result = OTUseSyncIdleEvents(endpoint, true);
		assertp("OTUseSyncIdleEvents failed", result == noErr);

		
		err = OTBind(endpoint, nil, nil);
		assertp("OTBind failed", err == noErr);
	}

	// set up address struct and connect
	if (err == noErr)
	{
		OTMemzero(&sndCall, sizeof(TCall));

		sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
		sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

		err = OTConnect(endpoint, &sndCall, nil);
		assertp("OTConnect failed", err == noErr);
	}

	printf("OT setup done\n");

	// init libssh2
	rc = libssh2_init(0);
	assertp("init fail", rc == 0);


	session = libssh2_session_init();
	assertp("session setup fail", !session);
/*
	// I think we stubbed this function out, lol
	//libssh2_session_set_blocking(session, endpoint);

	// replace 0 with the OT connection
	while((rc = libssh2_session_handshake(session, endpoint)) == LIBSSH2_ERROR_EAGAIN);
	assertp("handshake failed", !rc);

	// are we required to look at the known hosts? see if we can skip for now

	while((rc = libssh2_userauth_password(session, username, password)) == LIBSSH2_ERROR_EAGAIN);
	assertp("pw login failed", !rc);

	while((channel = libssh2_channel_open_session(session)) == NULL && libssh2_session_last_error(session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)
	{
		;//waitsocket(sock, session); // need this?
	}
	assertp("channel open failed", channel != 0);

	while((rc = libssh2_channel_exec(channel, command)) == LIBSSH2_ERROR_EAGAIN);
	assertp("exec channel open failed", rc == 0);


	// TODO: do the actual read here!


	while((rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN);
	assertp("channel close failed", rc == 0);

	// skipping some other stuff here

	libssh2_channel_free(channel);
	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
	*/
	libssh2_session_free(session);

	libssh2_exit();

	// OT cleanup
	OSStatus result = OTUnbind(endpoint);
	assertp("OTUnbind failed", result == noErr);

	// if we set up an endpoint, close it
	if (endpoint != kOTInvalidEndpointRef)
	{
		OSStatus result = OTCloseProvider(endpoint);
		assertp("OTCloseProvider failed", result == noErr);
	}

	// if we got a buffer, release it
	if (buffer != nil) OTFreeMem(buffer);

	return;
}

int main(int argc, char** argv)
{
	printf("hello, world\n");
	getchar();

	do_ssh_connection();

	printf("\n(return to exit)\n");
	getchar();

	return 0;
}

