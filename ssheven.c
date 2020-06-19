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
	if (!b) printf("assert fail: %s (%d)\n", message, b);
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
	char* hostname = "10.0.2.2:22";
	char* username = "ssheven";
	char* password = "password";
	char* command = "uname -a";

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
		printf("could not get memory!!!\n");
		return;
	}
	else
	{
		printf("got OT buffer\n");
	}

	// open TCP endpoint
	endpoint = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);
	assertp("endpoint opened", err == noErr);
	if (err != noErr) return;

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

	err = OTBind(endpoint, nil, nil);
	assertp("OTBind failed", err == noErr);

	if (err != noErr) return;

	// set up address struct and connect

	OTMemzero(&sndCall, sizeof(TCall));

	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

	err = OTConnect(endpoint, &sndCall, nil);
	assertp("OTConnect failed", err == noErr);

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

	rc = libssh2_session_handshake(session, endpoint);
	printf("handshake rc %s\n", libssh2_error_string(rc));
	if (rc != LIBSSH2_ERROR_NONE) return;

	rc = libssh2_userauth_password(session, username, password);
	printf("authenticate rc %s\n", libssh2_error_string(rc));
	if (rc != LIBSSH2_ERROR_NONE) return;

	channel = libssh2_channel_open_session(session);
	printf("channel open: %d\n", channel);

	printf("sending command \"%s\"\n", command);
	rc = libssh2_channel_exec(channel, command);
	printf("libssh2_channel_exec rc %s\n", libssh2_error_string(rc));

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

	rc = libssh2_channel_close(channel);
	printf("libssh2_channel_close rc %s\n", libssh2_error_string(rc));

	libssh2_channel_free(channel);

	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");

	libssh2_session_free(session);

	libssh2_exit();

	// request to close the TCP connection
	rc = OTSndOrderlyDisconnect(endpoint);
	assertp("OTSndOrderlyDisconnect failed", rc == noErr);

	// get any remaining data so we can finish closing the connection
	OTFlags ot_flags;
	rc = 1;
	while (rc != kOTLookErr)
	{
		rc = OTRcv(endpoint, buffer, 1, &ot_flags);
	}

	// finish closing the TCP connection
	OTResult look_result = OTLook(endpoint);
	switch (look_result)
	{
		case T_DISCONNECT:
			err = OTRcvDisconnect(endpoint, nil);
			break;

		default:
			printf("other connection error: %d\n", look_result);
			break;
	}

	// release endpoint
	result = OTUnbind(endpoint);
	assertp("OTUnbind failed", result == noErr);

	result = OTCloseProvider(endpoint);
	assertp("OTCloseProvider failed", result == noErr);

	// if we got a buffer, release it
	if (buffer != nil) OTFreeMem(buffer);

	return;
}

int main(int argc, char** argv)
{
	printf("starting up\n");

	if (InitOpenTransport() != noErr)
	{
		printf("failed to init OT \n");
		return 0;
	}

	do_ssh_connection();

	CloseOpenTransport();

	printf("\n(a to exit)\n");
	while (getchar() != 'a');

	return 0;
}

