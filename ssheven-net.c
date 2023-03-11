/* 
 * ssheven
 * 
 * Copyright (c) 2021 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#include "ssheven.h"
#include "ssheven-net.h"
#include "ssheven-console.h"
#include "ssheven-debug.h"

#include <errno.h>
#include <Script.h>
#include <Threads.h>

#include <mbedtls/base64.h>

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

// read from the channel and print to console
void ssh_read(void)
{
	ssize_t rc = libssh2_channel_read(ssh_con.channel, ssh_con.recv_buffer, SSHEVEN_BUFFER_SIZE);

	if (rc == 0) return;

	if (rc <= 0)
	{
		printf_i("channel read error: %s\r\n", libssh2_error_string(rc));
		read_thread_command = EXIT;
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

int ssh_setup_terminal(void)
{
	int rc = 0;

	SSH_CHECK(libssh2_channel_request_pty_ex(ssh_con.channel, prefs.terminal_string, (strlen(prefs.terminal_string)), NULL, 0, con.size_x, con.size_y, 0, 0));
	SSH_CHECK(libssh2_channel_shell(ssh_con.channel));

	read_thread_state = OPEN;

	return 1;
}

// returns base64 sha256 hash of key as a malloc'd pascal string
char* host_hash(void)
{
	size_t length = 0;
	char* human_readable = malloc(66);
	memset(human_readable, 0, 66);
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

		int e = libssh2_knownhost_readfile(known_hosts, known_hosts_file_path, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
		if (e < 0)
		{
			printf_i("Failed to load known hosts file: %s\r\n", libssh2_error_string(e));
		}
	}
	else
	{
		printf_i("No known hosts file found.\r\n");
	}

	if (safe_to_connect)
	{
		// hostnames need to be either plain or of the format "[host]:port"
		prefs.hostname[prefs.hostname[0]+1] = '\0';
		int e = libssh2_knownhost_check(known_hosts, prefs.hostname+1, host_key, key_len, LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW, NULL);
		prefs.hostname[prefs.hostname[0]+1] = ':';

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
				//printf_i("Host and key match found in known hosts.\r\n");
				recognized_key = 1;
				break;
			case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
				printf_i("WARNING! Host found in known hosts but key doesn't match!\r\n");
				safe_to_connect = 0;
				break;
			default:
				printf_i("Unexpected error while checking known-hosts: %d\r\n", e);
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

		int save_type = 0;
		save_type |= LIBSSH2_KNOWNHOST_TYPE_PLAIN;
		save_type |= LIBSSH2_KNOWNHOST_KEYENC_RAW;

		switch (key_type)
		{
			default:
				__attribute__ ((fallthrough));
			case LIBSSH2_HOSTKEY_TYPE_UNKNOWN:
				save_type |= LIBSSH2_KNOWNHOST_KEY_UNKNOWN;
				break;
			case LIBSSH2_HOSTKEY_TYPE_RSA:
				save_type |= LIBSSH2_KNOWNHOST_KEY_SSHRSA;
				break;
			case LIBSSH2_HOSTKEY_TYPE_DSS:
				save_type |= LIBSSH2_KNOWNHOST_KEY_SSHDSS;
				break;
			case LIBSSH2_HOSTKEY_TYPE_ECDSA_256:
				save_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_256;
				break;
			case LIBSSH2_HOSTKEY_TYPE_ECDSA_384:
				save_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_384;
				break;
			case LIBSSH2_HOSTKEY_TYPE_ECDSA_521:
				save_type |= LIBSSH2_KNOWNHOST_KEY_ECDSA_521;
				break;
			case LIBSSH2_HOSTKEY_TYPE_ED25519:
				save_type |= LIBSSH2_KNOWNHOST_KEY_ED25519;
				break;
		}

		// hostnames need to be either plain or of the format "[host]:port"
		prefs.hostname[prefs.hostname[0]+1] = '\0';
		int e = libssh2_knownhost_addc(known_hosts, prefs.hostname+1, NULL, host_key, key_len, NULL, 0, save_type, NULL);
		prefs.hostname[prefs.hostname[0]+1] = ':';

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


ssize_t network_recv_callback(libssh2_socket_t sock, void *buffer,
               size_t length, int flags, void **abstract)
{
	OTResult ret = kOTNoDataErr;
	OTFlags ot_flags = 0;

	if (length == 0) return 0;

	// in non-blocking mode, returns instantly always
	ret = OTRcv(ssh_con.endpoint, buffer, length, &ot_flags);

	// if we got bytes, return them
	if (ret >= 0) return ret;

	// if no data, let other threads run, then tell caller to call again
	if (ret == kOTNoDataErr && read_thread_command != EXIT)
	{
		YieldToAnyThread();
		return -EAGAIN;
	}

	// if we got anything other than data or nothing, return an error
	if (ret != kOTNoDataErr) return -1;

	return -1;
}

ssize_t network_send_callback(libssh2_socket_t sock, const void *buffer,
               size_t length, int flags, void **abstract)
{
    int ret = -1;

    ret = OTSnd(ssh_con.endpoint, (void*) buffer, length, 0);

    // TODO FIXME handle cases better, i.e. translate error cases
    if (ret == kOTLookErr)
    {
        OTResult lookresult = OTLook(ssh_con.endpoint);
        //printf("kOTLookErr, reason: %ld\n", lookresult);

        switch (lookresult)
        {
            default:
               //printf("what?\n");
               ret = -1;
               break;
        }
    }

    return (ssize_t) ret;
}

void ssh_end_msg_callback(LIBSSH2_SESSION* session, int reason, const char *message,
           int message_len, const char *language, int language_len,
           void **abstract)
{
	printf_i("got a disconnect msg\r\n");
}

int init_connection(char* hostname)
{
	int rc;

	// OT vars
	OSStatus err = noErr;
	TCall sndCall;
	DNSAddress hostDNSAddress;


	// open TCP endpoint
	ssh_con.endpoint = OTOpenEndpoint(OTCreateConfiguration(kTCPName), 0, nil, &err);

	if (err != noErr || ssh_con.endpoint == kOTInvalidEndpointRef)
	{
		printf_i("Failed to open Open Transport TCP endpoint.\r\n");
		return 0;
	}

	OT_CHECK(OTSetSynchronous(ssh_con.endpoint));
	OT_CHECK(OTSetBlocking(ssh_con.endpoint));
	OT_CHECK(OTUseSyncIdleEvents(ssh_con.endpoint, false));

	OT_CHECK(OTBind(ssh_con.endpoint, nil, nil));

	OT_CHECK(OTSetNonBlocking(ssh_con.endpoint));

	// set up address struct, do the DNS lookup, and connect
	OTMemzero(&sndCall, sizeof(TCall));

	sndCall.addr.buf = (UInt8 *) &hostDNSAddress;
	sndCall.addr.len = OTInitDNSAddress(&hostDNSAddress, (char *) hostname);

	printf_i("Connecting endpoint... "); YieldToAnyThread();
	OT_CHECK(OTConnect(ssh_con.endpoint, &sndCall, nil));
	printf_i("done.\r\n"); YieldToAnyThread();

	// init libssh2
	SSH_CHECK(libssh2_init(0));
	YieldToAnyThread();

	ssh_con.session = libssh2_session_init();
	if (ssh_con.session == 0)
	{
		printf_i("Failed to initialize SSH session.\r\n");
		return 0;
	}
	YieldToAnyThread();

	// register callbacks
	libssh2_session_callback_set(ssh_con.session, LIBSSH2_CALLBACK_SEND, network_send_callback);
	libssh2_session_callback_set(ssh_con.session, LIBSSH2_CALLBACK_RECV, network_recv_callback);
	libssh2_session_callback_set(ssh_con.session, LIBSSH2_CALLBACK_DISCONNECT, network_recv_callback);

	long s = TickCount();
	printf_i("Beginning SSH session handshake... "); YieldToAnyThread();
	SSH_CHECK(libssh2_session_handshake(ssh_con.session, 0));

	printf_i("done. (%d ticks)\r\n", TickCount() - s); YieldToAnyThread();

	//const char* banner = libssh2_session_banner_get(ssh_con.session);
	//if (banner) printf_i("Server banner: %s\r\n", banner);

	return 1;
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

	if (!ok)
	{
		read_thread_state = DONE;
		return 0;
	}

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

	if (libssh2_channel_eof(ssh_con.channel))
	{
		printf_i("(disconnected by server)");
	}

	// if we still have a connection, close it
	if (read_thread_state != DONE) end_connection();

	// disallow pasting after connection is closed
	DisableItem(menu, 5);

    // If the remote server closes the client connection unexpectedly, call "disconnect()
    // to release memory and avoid the user having to manually select Disconnect and then
    // Connect.. from the file menu.

    disconnect();


	return 0;
}
