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

const char* libssh2_error_string(int i)
{
	switch (i)
	{
		case LIBSSH2_ERROR_NONE:
			return "no error (LIBSSH2_ERROR_NONE)";
		case LIBSSH2_ERROR_BANNER_RECV:
			return "LIBSSH2_ERROR_BANNER_RECV";
		case LIBSSH2_ERROR_BANNER_SEND:
			return "LIBSSH2_ERROR_BANNER_SEND";
		case LIBSSH2_ERROR_INVALID_MAC:
			return "LIBSSH2_ERROR_INVALID_MAC";
		case LIBSSH2_ERROR_KEX_FAILURE:
			return "LIBSSH2_ERROR_KEX_FAILURE";
		case LIBSSH2_ERROR_ALLOC:
			return "LIBSSH2_ERROR_ALLOC";
		case LIBSSH2_ERROR_SOCKET_SEND:
			return "LIBSSH2_ERROR_SOCKET_SEND";
		case LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE:
			return "LIBSSH2_ERROR_KEY_EXCHANGE_FAILURE";
		case LIBSSH2_ERROR_TIMEOUT:
			return "LIBSSH2_ERROR_TIMEOUT";
		case LIBSSH2_ERROR_HOSTKEY_INIT:
			return "LIBSSH2_ERROR_HOSTKEY_INIT";
		case LIBSSH2_ERROR_HOSTKEY_SIGN:
			return "LIBSSH2_ERROR_HOSTKEY_SIGN";
		case LIBSSH2_ERROR_DECRYPT:
			return "LIBSSH2_ERROR_DECRYPT";
		case LIBSSH2_ERROR_SOCKET_DISCONNECT:
			return "LIBSSH2_ERROR_SOCKET_DISCONNECT";
		case LIBSSH2_ERROR_PROTO:
			return "LIBSSH2_ERROR_PROTO";
		case LIBSSH2_ERROR_PASSWORD_EXPIRED:
			return "LIBSSH2_ERROR_PASSWORD_EXPIRED";
		case LIBSSH2_ERROR_FILE:
			return "LIBSSH2_ERROR_FILE";
		case LIBSSH2_ERROR_METHOD_NONE:
			return "LIBSSH2_ERROR_METHOD_NONE";
		case LIBSSH2_ERROR_AUTHENTICATION_FAILED:
			return "LIBSSHLIBSSH2_ERROR_AUTHENTICATION_FAILED2_ERROR_NONE";
		case LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED:
			return "LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED";
		case LIBSSH2_ERROR_CHANNEL_OUTOFORDER:
			return "LIBSSH2_ERROR_CHANNEL_OUTOFORDER";
		case LIBSSH2_ERROR_CHANNEL_FAILURE:
			return "LIBSSH2_ERROR_CHANNEL_FAILURE";
		case LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED:
			return "LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED";
		case LIBSSH2_ERROR_CHANNEL_UNKNOWN:
			return "LIBSSH2_ERROR_CHANNEL_UNKNOWN";
		case LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED:
			return "LIBSSH2_ERROR_CHANNEL_WINDOW_EXCEEDED";
		case LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED:
			return "LIBSSH2_ERROR_CHANNEL_PACKET_EXCEEDED";
		case LIBSSH2_ERROR_CHANNEL_CLOSED:
			return "LIBSSH2_ERROR_CHANNEL_CLOSED";
		case LIBSSH2_ERROR_CHANNEL_EOF_SENT:
			return "LIBSSH2_ERROR_CHANNEL_EOF_SENT";
		case LIBSSH2_ERROR_SCP_PROTOCOL:
			return "LIBSSH2_ERROR_SCP_PROTOCOL";
		case LIBSSH2_ERROR_ZLIB:
			return "LIBSSH2_ERROR_ZLIB";
		case LIBSSH2_ERROR_SOCKET_TIMEOUT:
			return "LIBSSH2_ERROR_SOCKET_TIMEOUT";
		case LIBSSH2_ERROR_SFTP_PROTOCOL:
			return "LIBSSH2_ERROR_SFTP_PROTOCOL";
		case LIBSSH2_ERROR_REQUEST_DENIED:
			return "LIBSSH2_ERROR_REQUEST_DENIED";
		case LIBSSH2_ERROR_METHOD_NOT_SUPPORTED:
			return "LIBSSH2_ERROR_METHOD_NOT_SUPPORTED";
		case LIBSSH2_ERROR_INVAL:
			return "LIBSSH2_ERROR_INVAL";
		case LIBSSH2_ERROR_INVALID_POLL_TYPE:
			return "LIBSSH2_ERROR_INVALID_POLL_TYPE";
		case LIBSSH2_ERROR_PUBLICKEY_PROTOCOL:
			return "LIBSSH2_ERROR_PUBLICKEY_PROTOCOL";
		case LIBSSH2_ERROR_EAGAIN:
			return "LIBSSH2_ERROR_EAGAIN";
		case LIBSSH2_ERROR_BUFFER_TOO_SMALL:
			return "LIBSSH2_ERROR_BUFFER_TOO_SMALL";
		case LIBSSH2_ERROR_BAD_USE:
			return "LIBSSH2_ERROR_BAD_USE";
		case LIBSSH2_ERROR_COMPRESS:
			return "LIBSSH2_ERROR_COMPRESS";
		case LIBSSH2_ERROR_OUT_OF_BOUNDARY:
			return "LIBSSH2_ERROR_OUT_OF_BOUNDARY";
		case LIBSSH2_ERROR_AGENT_PROTOCOL:
			return "LIBSSH2_ERROR_SOCKET_RECV";
		case LIBSSH2_ERROR_SOCKET_RECV:
			return "LIBSSH2_ERROR_ENCRYPT";
		case LIBSSH2_ERROR_ENCRYPT:
			return "LIBSSH2_ERROR_ENCRYPT";
		case LIBSSH2_ERROR_BAD_SOCKET:
			return "LIBSSH2_ERROR_BAD_SOCKET";
		case LIBSSH2_ERROR_KNOWN_HOSTS:
			return "LIBSSH2_ERROR_KNOWN_HOSTS";
		case LIBSSH2_ERROR_CHANNEL_WINDOW_FULL:
			return "LIBSSH2_ERROR_CHANNEL_WINDOW_FULL";
		case LIBSSH2_ERROR_KEYFILE_AUTH_FAILED:
			return "LIBSSH2_ERROR_KEYFILE_AUTH_FAILED";

		default:
			return "unknown error number";
	}

	return "what???";
}

enum { buffer_size = 4096 };

void assertp(char* message, int b)
{
	if (!b) { printf("assert fail: %s (%d)\n", message, b); fflush(stdout); }
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
		printf("could not get memory!!!\n");fflush(stdout);
		return;
	}
	else
	{
		printf("got mem\n");fflush(stdout);
	}

	// open TCP endpoint
	endpoint = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);
	assertp("endpoint opened", err == noErr);
	if (err != noErr) return;

	printf("0\n");fflush(stdout);

	// configure the endpoint
	// synchronous and blocking, and we yield until we get a result

	OSStatus result;

	result = OTSetSynchronous(endpoint);
	assertp("OTSetSynchronous failed", result == noErr);

	result = OTSetBlocking(endpoint);
	assertp("OTSetBlocking failed", result == noErr);

	result = OTInstallNotifier(endpoint, yield_notifier, nil);
	assertp("OTInstallNotifier failed", result == noErr);

	result = OTUseSyncIdleEvents(endpoint, true);
	assertp("OTUseSyncIdleEvents failed", result == noErr);

	printf("0.5\n");fflush(stdout);

	err = OTBind(endpoint, nil, nil);
	assertp("OTBind failed", err == noErr);

	if (err != noErr) return;

	printf("1\n");fflush(stdout);

	// set up address struct and connect

	OTMemzero(&sndCall, sizeof(TCall));

	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

	err = OTConnect(endpoint, &sndCall, nil);
	assertp("OTConnect failed", err == noErr);

	printf("2\n");fflush(stdout);

	if (err != noErr) return;

	printf("OT setup done, endpoint: %d, (should not be %d for libssh2, should not be %d for OT)\n", (int) endpoint, (int) LIBSSH2_INVALID_SOCKET, (int) kOTInvalidEndpointRef);

	// init libssh2
	rc = libssh2_init(0);
	printf("init rc %s\n", libssh2_error_string(rc));

	session = libssh2_session_init();
	if (session != 0)
	{
		printf("session ok\n");
	}
	else
	{
		printf("session fail\n");
		return;
	}

	printf("3\n");fflush(stdout);

	// I think we stubbed this function out, lol
	//libssh2_session_set_blocking(session, endpoint);

	// replace 0 with the OT connection
	rc = libssh2_session_handshake(session, endpoint);
	printf("handshake rc %s\n", libssh2_error_string(rc));
	if (rc != LIBSSH2_ERROR_NONE) return;
	//while((rc = libssh2_session_handshake(session, endpoint)) == LIBSSH2_ERROR_EAGAIN);

	printf("3.5\n");fflush(stdout);

	//rc = libssh2_userauth_password(session, username, password);
	printf("authenticate rc %s\n", libssh2_error_string(rc));
	if (rc != LIBSSH2_ERROR_NONE) return;

/*
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
*/
	//libssh2_channel_free(channel);

	printf("4\n");fflush(stdout);

	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");

	printf("5\n");fflush(stdout);

	libssh2_session_free(session);

	printf("6\n");fflush(stdout);

	libssh2_exit();fflush(stdout);

	// OT cleanup
	result = OTUnbind(endpoint);
	assertp("OTUnbind failed", result == noErr);

	printf("7\n");

	// if we set up an endpoint, close it
	if (endpoint != kOTInvalidEndpointRef)
	{
		OSStatus result = OTCloseProvider(endpoint);
		assertp("OTCloseProvider failed", result == noErr);
	}

	// if we got a buffer, release it
	if (buffer != nil) OTFreeMem(buffer);

	printf("8\n");

	return;
}

int main(int argc, char** argv)
{
	printf("hello, world\n");
	getchar();

	if (InitOpenTransport() != noErr)
	{
		printf("failed to init OT \n");
		return 0;
	}

	do_ssh_connection();

	CloseOpenTransport();

	printf("\n(return to exit)\n");
	while (getchar() != 'a');

	return 0;
}

