/* 
 * ssheven
 * 
 * Copyright (c) 2021 by cy384 <cy384@cy384.com>
 * See LICENSE file for details
 */

#define OT_CHECK(X) err = (X); if (err != noErr) { printf_i("" #X " failed\r\n"); return 0; };
#define SSH_CHECK(X) rc = (X); if (rc != LIBSSH2_ERROR_NONE) { printf_i("" #X " failed: %s\r\n", libssh2_error_string(rc)); return 0;};

const char* libssh2_error_string(int i);
const char* OT_event_string(int i);
