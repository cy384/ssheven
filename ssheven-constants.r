/* used as both a C and resource file include */

#ifndef __SSHEVEN_CONSTANTS_R__
#define __SSHEVEN_CONSTANTS_R__

/* so many versions */
#define SSHEVEN_VERSION             "0.5.0"
#define SSHEVEN_LONG_VERSION        "0.5.0 prerelease, by cy384"
#define SSHEVEN_DESCRIPTION         "ssheven 0.4.0 by cy384"
#define SSHEVEN_VERSION_MAJOR       0x00
#define SSHEVEN_VERSION_MINOR       0x50
#define SSHEVEN_VERSION_PRERELEASE  0x01

/* options: development, alpha, beta, release */
#define SSHEVEN_RELEASE_TYPE        development
#define SSHEVEN_RELEASE_REGION      verUS

/* requested number of bytes for RAM, used in SIZE resource */
#define SSHEVEN_MINIMUM_PARTITION   2048*1024
#define SSHEVEN_REQUIRED_PARTITION  SSHEVEN_MINIMUM_PARTITION

/* size in bytes for recv and send thread buffers */
#define SSHEVEN_BUFFER_SIZE 4096

/* terminal type to send over ssh, determines features etc.
 * "vanilla" supports basically nothing, "vt100" basics, "xterm" everything */
#define SSHEVEN_TERMINAL_TYPE "xterm"

/* dialog for getting connection info */
#define DLOG_CONNECT 128
#define DITL_CONNECT 128

/* alert for failure to find OT */
#define ALRT_OT 129
#define DITL_OT 129

/* alert for failure to find thread manager */
#define ALRT_TM 130
#define DITL_TM 130

/* alert for slow CPU detected */
#define ALRT_CPU_SLOW 131
#define DITL_CPU_SLOW 131

/* alert for pre-68020 detected */
#define ALRT_CPU_BAD 132
#define DITL_CPU_BAD 132

/* about info window */
#define DLOG_ABOUT 133
#define DITL_ABOUT 133
#define PICT_ABOUT 133

/* password entry window */
#define DLOG_PASSWORD 134
#define DITL_PASSWORD 134

/* alert for password authentication failure */
#define ALRT_PW_FAIL 135
#define DITL_PW_FAIL 135

/* menus */
#define MBAR_SSHEVEN 128
#define MENU_APPLE   128
#define MENU_FILE    129
#define MENU_EDIT    130

#endif
