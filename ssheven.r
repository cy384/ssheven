#include "ssheven-constants.r"

#include "Dialogs.r"

resource 'DLOG' (DLOG_ABOUT, purgeable) {
	{0, 0, 148, 420},
	dBoxProc,
	visible,
	goAway,
	0,
	DITL_ABOUT,
	"",
	centerMainScreen
};

resource 'DITL' (DITL_ABOUT, purgeable) {
	{
		{ 10, 10, 138, 138 },
		Picture { enabled, PICT_ABOUT };

		{ 34, 160, 54, 410},
		StaticText { disabled, "ssheven" };

		{ 64, 160, 84, 410},
		StaticText { disabled, SSHEVEN_LONG_VERSION };

		{ 94, 160, 114, 410},
		StaticText { disabled, "https://github.com/cy384/ssheven" };
	}
};

resource 'DLOG' (DLOG_CONNECT) {
	{ 50, 100, 195, 420 },
	dBoxProc,
	visible,
	noGoAway,
	0,
	DLOG_CONNECT,
	"",
	centerMainScreen
};

resource 'DITL' (DITL_CONNECT) {
	{
		{ 115, 320-10-80, 135, 320-10 },
		Button { enabled, "Connect" };

		{ 190-10-20-5, 320-10-80-5, 190-10+5, 320-10+5 },
		UserItem { enabled };

		{ 10, 10, 30, 310 },
		StaticText { enabled, "Address with port" };

		{ 35, 15, 51, 305 },
		EditText { enabled, "10.0.2.2:22" };

		{ 60, 10, 80, 310 },
		StaticText { enabled, "Username" };

		{ 85, 15, 101, 305 },
		EditText { enabled, "" };

		{ 115, 10, 135, 90 },
		Button { enabled, "Cancel" };
	}
};

resource 'DITL' (DITL_OT) {
	{
		{ 50, 260, 70, 340 },
		Button { enabled, "Exit" };

		{ 10, 70, 30, 340 },
		StaticText { enabled, "Open Transport required but not found!" };
	}
};

resource 'ALRT' (ALRT_OT, purgeable) {
	{ 50, 100, 50+80, 100+350 },
	DITL_OT,

	/* OK means draw default border on first button */
	{
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (DITL_TM) {
	{
		{ 50, 260, 70, 340 },
		Button { enabled, "Exit" };

		{ 10, 70, 30, 340 },
		StaticText { enabled, "Thread Manager required but not found!" };
	}
};

resource 'ALRT' (ALRT_TM, purgeable) {
	{ 50, 100, 50+80, 100+350 },
	ALRT_TM,

	/* OK means draw default border on first button */
	{
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (DITL_CPU_SLOW) {
	{
		{ 50, 260, 70, 340 },
		Button { enabled, "OK" };

		{ 10, 70, 30, 340 },
		StaticText { enabled, "Your CPU is probably too slow!" };
	}
};

resource 'ALRT' (ALRT_CPU_SLOW, purgeable) {
	{ 50, 100, 50+80, 100+350 },
	DITL_CPU_SLOW,

	/* OK means draw default border on first button */
	{
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DITL' (DITL_CPU_BAD) {
	{
		{ 50, 260, 70, 340 },
		Button { enabled, "OK" };

		{ 10, 70, 30, 340 },
		StaticText { enabled, "SSHeven requires a 68020 or later!" };
	}
};

resource 'ALRT' (ALRT_CPU_BAD, purgeable) {
	{ 50, 100, 50+80, 100+350 },
	DITL_CPU_BAD,

	/* OK means draw default border on first button */
	{
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent,
		OK, visible, silent
	},
	alertPositionMainScreen
};

resource 'DLOG' (DLOG_PASSWORD) {
	{ 50, 100, 150, 420 },
	dBoxProc,
	visible,
	noGoAway,
	0,
	DLOG_PASSWORD,
	"",
	centerMainScreen
};

resource 'DITL' (DITL_PASSWORD) {
	{
		{ 70, 320-10-80, 90, 320-10 },
		Button { enabled, "OK" };

		{ 190-10-20-5, 320-10-80-5, 190-10+5, 320-10+5 },
		UserItem { enabled };

		{ 10, 10, 30, 310 },
		StaticText { enabled, "Enter password:" };

		{ 35, 15, 51, 305 },
		EditText { enabled, "" };

		{ 240, 10, 240, 10 },
		EditText { enabled, "" };

		{ 70, 10, 90, 90 },
		Button { enabled, "Cancel" };
	}
};

#include "Processes.r"

resource 'SIZE' (-1) {
	reserved,
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	SSHEVEN_MINIMUM_PARTITION,
	SSHEVEN_REQUIRED_PARTITION
};


#include "MacTypes.r"
/* see macintosh tb essentials page 7-31 */
/* yes, we need two */
/* first one displayed in version field of info window */
resource 'vers' (1, purgeable) {
	SSHEVEN_VERSION_MAJOR, SSHEVEN_VERSION_MINOR,
	SSHEVEN_RELEASE_TYPE, SSHEVEN_VERSION_PRERELEASE,
	SSHEVEN_RELEASE_REGION,
	SSHEVEN_VERSION,
	SSHEVEN_LONG_VERSION
};

/* second one displayed beneath icon at top of info window */
resource 'vers' (2, purgeable) {
	SSHEVEN_VERSION_MAJOR, SSHEVEN_VERSION_MINOR,
	SSHEVEN_RELEASE_TYPE, SSHEVEN_VERSION_PRERELEASE,
	SSHEVEN_RELEASE_REGION,
	SSHEVEN_VERSION,
	SSHEVEN_LONG_VERSION
};

/* signature resource */
type 'SSH7' as 'STR ';
resource 'SSH7' (0, purgeable) {
	SSHEVEN_DESCRIPTION
};

#include "Finder.r"
resource 'FREF' (128, purgeable) {
	'APPL', 0, ""
};

resource 'BNDL' (128, purgeable) {
	'SSH7', 0,
	{
		'ICN#', {0, 128},
		'FREF', {0, 128}
	}
};

#include "Menus.r"

resource 'MBAR' (MBAR_SSHEVEN, preload)
{
	{ MENU_APPLE, MENU_FILE, MENU_EDIT };
};

resource 'MENU' (MENU_APPLE) {
	MENU_APPLE, textMenuProc;
	allEnabled, enabled;
	apple;
	{
		"About ssheven...", noIcon, noKey, noMark, plain;
		"-", noIcon, noKey, noMark, plain;
	}
};

resource 'MENU' (MENU_FILE) {
	MENU_FILE, textMenuProc;
	allEnabled, enabled;
	"File";
	{
		"Quit", noIcon, "Q", noMark, plain;
	}
};

resource 'MENU' (MENU_EDIT) {
	MENU_EDIT, textMenuProc;
	allEnabled, enabled;
	"Edit";
	{
		"Undo", noIcon, "Z", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Cut", noIcon, "X", noMark, plain;
		"Copy", noIcon, "C", noMark, plain;
		"Paste", noIcon, "V", noMark, plain;
		"Clear", noIcon, noKey, noMark, plain;
		"Select All", noIcon, "A", noMark, plain;
		"-", noIcon, noKey, noMark, plain;
		"Show Clipboard", noIcon, noKey, noMark, plain;
	}
};

#include "Icons.r"

/* use this regex to get rid of data comments: "            \/\*[^\*]*\*\/" */

data 'ICN#' (128) {
	$"1FFF FFF8 0FFF FFF0 07FF FFF0 03FF FFE0"
	$"0000 03E0 0000 07C0 0000 07C0 0000 0F80"
	$"0000 0F80 0000 1F00 0000 1F00 0000 3E00"
	$"0180 3E00 0240 7C00 0420 7C00 099F FFE0"
	$"0A40 0010 0A40 0010 099F FEA0 0423 E2A0"
	$"0243 E140 0187 C000 0007 C000 000F 8000"
	$"000F 8000 001F 0000 001F 0000 003E 0000"
	$"003E 0000 007C 0000 007C 0000 00F8 0000"
	$"1FFF FFF8 0FFF FFF0 07FF FFF0 03FF FFE0"
	$"0000 03E0 0000 07C0 0000 07C0 0000 0F80"
	$"0000 0F80 0000 1F00 0000 1F00 0000 3E00"
	$"0180 3E00 03C0 7C00 07E0 7C00 0FFF FFE0"
	$"0E7F FFF0 0E7F FFF0 0FFF FFE0 07E3 E3E0"
	$"03C3 E140 0187 C000 0007 C000 000F 8000"
	$"000F 8000 001F 0000 001F 0000 003E 0000"
	$"003E 0000 007C 0000 007C 0000 00F8 0000"
};

data 'icl4' (128) {
	$"000F FFFF FFFF FFFF FFFF FFFF FFFF F000"
	$"0000 FFFF FFFF FFFF FFFF FFFF FFFF 0000"
	$"0000 0FFF FFFF FFFF FFFF FFFF FFFF 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF FFF0 0000"
	$"0000 0000 0000 0000 0000 00FF FFF0 0000"
	$"0000 0000 0000 0000 0000 0FFF FF00 0000"
	$"0000 0000 0000 0000 0000 0FFF FF00 0000"
	$"0000 0000 0000 0000 0000 FFFF F000 0000"
	$"0000 0000 0000 0000 0000 FFFF F000 0000"
	$"0000 0000 0000 0000 000F FFFF 0000 0000"
	$"0000 0000 0000 0000 000F FFFF 0000 0000"
	$"0000 0000 0000 0000 00FF FFF0 0000 0000"
	$"0000 000F F000 0000 00FF FFF0 0000 0000"
	$"0000 00F1 1F00 0000 0FFF FF00 0000 0000"
	$"0000 0F11 11F0 0000 0FFF FF00 0000 0000"
	$"0000 F11F F11F FFFF FFFF FFFF FFF0 0000"
	$"0000 F1F0 0F11 1111 1111 1111 111F 0000"
	$"0000 F1F0 0F12 2222 2222 2222 222F 0000"
	$"0000 F21F F12F FFFF FFFF FFF1 F1F0 0000"
	$"0000 0F21 12F0 00FF FFF0 00F2 F2F0 0000"
	$"0000 00F2 2F00 00FF FFF0 000F 0F00 0000"
	$"0000 000F F000 0FFF FF00 0000 0000 0000"
	$"0000 0000 0000 0FFF FF00 0000 0000 0000"
	$"0000 0000 0000 FFFF F000 0000 0000 0000"
	$"0000 0000 0000 FFFF F000 0000 0000 0000"
	$"0000 0000 000F FFFF 0000 0000 0000 0000"
	$"0000 0000 000F FFFF 0000 0000 0000 0000"
	$"0000 0000 00FF FFF0 0000 0000 0000 0000"
	$"0000 0000 00FF FFF0 0000 0000 0000 0000"
	$"0000 0000 0FFF FF00 0000 0000 0000 0000"
	$"0000 0000 0FFF FF00 0000 0000 0000 0000"
	$"0000 0000 FFFF F000 0000 0000 0000 0000"
};

data 'icl8' (128) {
	$"0000 00FF FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF FF00 0000"
	$"0000 0000 FFFF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 0000 00FF FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FFFF 0000 0000"
	$"0000 0000 0000 FFFF FFFF FFFF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FF00 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 FFFF FFFF FF00 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 00FF FFFF FFFF 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 00FF FFFF FFFF 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 FFFF FFFF FF00 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 FFFF FFFF FF00 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 00FF FFFF FFFF 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 00FF FFFF FFFF 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 FFFF FFFF FF00 0000 0000 0000 0000"
	$"0000 0000 0000 00FF FF00 0000 0000 0000"
	$"0000 FFFF FFFF FF00 0000 0000 0000 0000"
	$"0000 0000 0000 FF05 05FF 0000 0000 0000"
	$"00FF FFFF FFFF 0000 0000 0000 0000 0000"
	$"0000 0000 00FF 0505 0505 FF00 0000 0000"
	$"00FF FFFF FFFF 0000 0000 0000 0000 0000"
	$"0000 0000 FF05 05FF FF05 05FF FFFF FFFF"
	$"FFFF FFFF FFFF FFFF FFFF FF00 0000 0000"
	$"0000 0000 FF05 FF00 00FF 0505 0505 0505"
	$"0505 0505 0505 0505 0505 05FF 0000 0000"
	$"0000 0000 FF05 FF00 00FF 0516 1616 1616"
	$"1616 1616 1616 1616 1616 16FF 0000 0000"
	$"0000 0000 FF16 05FF FF05 16FF FFFF FFFF"
	$"FFFF FFFF FFFF FF05 FF05 FF00 0000 0000"
	$"0000 0000 00FF 1605 0516 FF00 0000 FFFF"
	$"FFFF FF00 0000 FF16 FF16 FF00 0000 0000"
	$"0000 0000 0000 FF16 16FF 0000 0000 FFFF"
	$"FFFF FF00 0000 00FF 00FF 0000 0000 0000"
	$"0000 0000 0000 00FF FF00 0000 00FF FFFF"
	$"FFFF 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 00FF FFFF"
	$"FFFF 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 FFFF FFFF"
	$"FF00 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 0000 FFFF FFFF"
	$"FF00 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 00FF FFFF FFFF"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 00FF FFFF FFFF"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 FFFF FFFF FF00"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 0000 FFFF FFFF FF00"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 00FF FFFF FFFF 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 00FF FFFF FFFF 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
	$"0000 0000 0000 0000 FFFF FFFF FF00 0000"
	$"0000 0000 0000 0000 0000 0000 0000 0000"
};

data 'ics#' (128) {
	$"3FFC 1FF8 0018 0030 0030 0060 1860 27FC"
	$"27FC 1994 0180 0300 0300 0600 0600 0C00"
	$"3FFC 1FF8 0018 0030 0030 0060 1860 27FC"
	$"27FC 1994 0180 0300 0300 0600 0600 0C00"
};

data 'ics4' (128) {
	$"00FF FFFF FFFF FF00 000F FFFF FFFF F000"
	$"0000 0000 000F F000 0000 0000 00FF 0000"
	$"0000 0000 00FF 0000 0000 0000 0FF0 0000"
	$"0002 2000 0FF0 0000 0020 0222 2222 2200"
	$"0020 0222 2222 2200 0002 200F F002 0200"
	$"0000 000F F000 0000 0000 00FF 0000 0000"
	$"0000 00FF 0000 0000 0000 0FF0 0000 0000"
	$"0000 0FF0 0000 0000 0000 FF00 0000 0000"
};

data 'ics8' (128) {
	$"0000 FFFF FFFF FFFF FFFF FFFF FFFF 0000"
	$"0000 00FF FFFF FFFF FFFF FFFF FF00 0000"
	$"0000 0000 0000 0000 0000 00FF FF00 0000"
	$"0000 0000 0000 0000 0000 FFFF 0000 0000"
	$"0000 0000 0000 0000 0000 FFFF 0000 0000"
	$"0000 0000 0000 0000 00FF FF00 0000 0000"
	$"0000 0017 1700 0000 00FF FF00 0000 0000"
	$"0000 1700 0017 1717 1717 1717 1717 0000"
	$"0000 1700 0017 1717 1717 1717 1717 0000"
	$"0000 0017 1700 00FF FF00 0017 0017 0000"
	$"0000 0000 0000 00FF FF00 0000 0000 0000"
	$"0000 0000 0000 FFFF 0000 0000 0000 0000"
	$"0000 0000 0000 FFFF 0000 0000 0000 0000"
	$"0000 0000 00FF FF00 0000 0000 0000 0000"
	$"0000 0000 00FF FF00 0000 0000 0000 0000"
	$"0000 0000 FFFF 0000 0000 0000 0000 0000"
};


data 'PICT' (PICT_ABOUT) {
	$"0A64 0000 0000 0020 0020 0011 02FF 0C00"
	$"FFFF FFFF 0000 0000 0000 0000 0020 0000"
	$"0020 0000 0000 0000 000C 001E 0007 0001"
	$"000A 0007 001E 0027 003E 0090 0004 0000"
	$"0000 0020 0020 0000 0000 0020 0020 0007"
	$"001E 0027 003E 0003 1FFF FFF8 0FFF FFF0"
	$"07FF FFF0 03FF FFE0 0000 03E0 0000 07C0"
	$"0000 07C0 0000 0F80 0000 0F80 0000 1F00"
	$"0000 1F00 0000 3E00 0180 3E00 03C0 7C00"
	$"07E0 7C00 0FFF FFE0 0E7F FFF0 0E7F FFF0"
	$"0FFF FFE0 07E3 E3E0 03C3 E140 0187 C000"
	$"0007 C000 000F 8000 000F 8000 001F 0000"
	$"001F 0000 003E 0000 003E 0000 007C 0000"
	$"007C 0000 00F8 0000 0098 8020 0000 0000"
	$"0020 0020 0000 0000 0000 0000 0048 0000"
	$"0048 0000 0000 0008 0001 0008 0000 0000"
	$"0001 7810 A020 0000 0000 0008 8000 00FF"
	$"0000 FFFF FFFF FFFF 0000 FFFF FFFF CCCC"
	$"0004 FFFF FFFF 9999 0004 FFFF FFFF 6666"
	$"0004 FFFF FFFF 3333 0000 FFFF FFFF 0000"
	$"0004 FFFF CCCC FFFF 0004 FFFF CCCC CCCC"
	$"0000 FFFF CCCC 9999 0004 FFFF CCCC 6666"
	$"0004 FFFF CCCC 3333 0004 FFFF CCCC 0000"
	$"0004 FFFF 9999 FFFF 0004 FFFF 9999 CCCC"
	$"0004 FFFF 9999 9999 0004 FFFF 9999 6666"
	$"0004 FFFF 9999 3333 0004 FFFF 9999 0000"
	$"0004 FFFF 6666 FFFF 0000 FFFF 6666 CCCC"
	$"0004 FFFF 6666 9999 0004 FFFF 6666 6666"
	$"0000 FFFF 6666 3333 0004 FFFF 6666 0000"
	$"0004 FFFF 3333 FFFF 0004 FFFF 3333 CCCC"
	$"0004 FFFF 3333 9999 0004 FFFF 3333 6666"
	$"0004 FFFF 3333 3333 0004 FFFF 3333 0000"
	$"0004 FFFF 0000 FFFF 0004 FFFF 0000 CCCC"
	$"0004 FFFF 0000 9999 0004 FFFF 0000 6666"
	$"0004 FFFF 0000 3333 0004 FFFF 0000 0000"
	$"0004 CCCC FFFF FFFF 0004 CCCC FFFF CCCC"
	$"0004 CCCC FFFF 9999 0004 CCCC FFFF 6666"
	$"0004 CCCC FFFF 3333 0004 CCCC FFFF 0000"
	$"0000 CCCC CCCC FFFF 0000 CCCC CCCC CCCC"
	$"0004 CCCC CCCC 9999 0004 CCCC CCCC 6666"
	$"0004 CCCC CCCC 3333 0004 CCCC CCCC 0000"
	$"0004 CCCC 9999 FFFF 0004 CCCC 9999 CCCC"
	$"0004 CCCC 9999 9999 0000 CCCC 9999 6666"
	$"0004 CCCC 9999 3333 0004 CCCC 9999 0000"
	$"0004 CCCC 6666 FFFF 0004 CCCC 6666 CCCC"
	$"0004 CCCC 6666 9999 0004 CCCC 6666 6666"
	$"0004 CCCC 6666 3333 0004 CCCC 6666 0000"
	$"0004 CCCC 3333 FFFF 0004 CCCC 3333 CCCC"
	$"0004 CCCC 3333 9999 0004 CCCC 3333 6666"
	$"0004 CCCC 3333 3333 0004 CCCC 3333 0000"
	$"0004 CCCC 0000 FFFF 0004 CCCC 0000 CCCC"
	$"0004 CCCC 0000 9999 0004 CCCC 0000 6666"
	$"0004 CCCC 0000 3333 0004 CCCC 0000 0000"
	$"0000 9999 FFFF FFFF 0004 9999 FFFF CCCC"
	$"0004 9999 FFFF 9999 0004 9999 FFFF 6666"
	$"0004 9999 FFFF 3333 0004 9999 FFFF 0000"
	$"0004 9999 CCCC FFFF 0004 9999 CCCC CCCC"
	$"0004 9999 CCCC 9999 0004 9999 CCCC 6666"
	$"0004 9999 CCCC 3333 0004 9999 CCCC 0000"
	$"0000 9999 9999 FFFF 0004 9999 9999 CCCC"
	$"0004 9999 9999 9999 0004 9999 9999 6666"
	$"0004 9999 9999 3333 0004 9999 9999 0000"
	$"0004 9999 6666 FFFF 0004 9999 6666 CCCC"
	$"0000 9999 6666 9999 0004 9999 6666 6666"
	$"0004 9999 6666 3333 0004 9999 6666 0000"
	$"0004 9999 3333 FFFF 0004 9999 3333 CCCC"
	$"0004 9999 3333 9999 0004 9999 3333 6666"
	$"0004 9999 3333 3333 0004 9999 3333 0000"
	$"0004 9999 0000 FFFF 0004 9999 0000 CCCC"
	$"0004 9999 0000 9999 0000 9999 0000 6666"
	$"0004 9999 0000 3333 0004 9999 0000 0000"
	$"0004 6666 FFFF FFFF 0004 6666 FFFF CCCC"
	$"0004 6666 FFFF 9999 0004 6666 FFFF 6666"
	$"0004 6666 FFFF 3333 0004 6666 FFFF 0000"
	$"0004 6666 CCCC FFFF 0004 6666 CCCC CCCC"
	$"0004 6666 CCCC 9999 0004 6666 CCCC 6666"
	$"0004 6666 CCCC 3333 0004 6666 CCCC 0000"
	$"0004 6666 9999 FFFF 0004 6666 9999 CCCC"
	$"0004 6666 9999 9999 0004 6666 9999 6666"
	$"0004 6666 9999 3333 0004 6666 9999 0000"
	$"0004 6666 6666 FFFF 0000 6666 6666 CCCC"
	$"0004 6666 6666 9999 0004 6666 6666 6666"
	$"0004 6666 6666 3333 0004 6666 6666 0000"
	$"0004 6666 3333 FFFF 0004 6666 3333 CCCC"
	$"0004 6666 3333 9999 0004 6666 3333 6666"
	$"0004 6666 3333 3333 0004 6666 3333 0000"
	$"0004 6666 0000 FFFF 0004 6666 0000 CCCC"
	$"0004 6666 0000 9999 0004 6666 0000 6666"
	$"0004 6666 0000 3333 0004 6666 0000 0000"
	$"0004 3333 FFFF FFFF 0004 3333 FFFF CCCC"
	$"0000 3333 FFFF 9999 0004 3333 FFFF 6666"
	$"0004 3333 FFFF 3333 0004 3333 FFFF 0000"
	$"0004 3333 CCCC FFFF 0004 3333 CCCC CCCC"
	$"0004 3333 CCCC 9999 0004 3333 CCCC 6666"
	$"0004 3333 CCCC 3333 0004 3333 CCCC 0000"
	$"0004 3333 9999 FFFF 0004 3333 9999 CCCC"
	$"0004 3333 9999 9999 0000 3333 9999 6666"
	$"0004 3333 9999 3333 0004 3333 9999 0000"
	$"0004 3333 6666 FFFF 0004 3333 6666 CCCC"
	$"0004 3333 6666 9999 0000 3333 6666 6666"
	$"0004 3333 6666 3333 0004 3333 6666 0000"
	$"0004 3333 3333 FFFF 0004 3333 3333 CCCC"
	$"0004 3333 3333 9999 0000 3333 3333 6666"
	$"0004 3333 3333 3333 0004 3333 3333 0000"
	$"0004 3333 0000 FFFF 0004 3333 0000 CCCC"
	$"0000 3333 0000 9999 0004 3333 0000 6666"
	$"0004 3333 0000 3333 0004 3333 0000 0000"
	$"0004 0000 FFFF FFFF 0004 0000 FFFF CCCC"
	$"0004 0000 FFFF 9999 0004 0000 FFFF 6666"
	$"0004 0000 FFFF 3333 0004 0000 FFFF 0000"
	$"0004 0000 CCCC FFFF 0004 0000 CCCC CCCC"
	$"0004 0000 CCCC 9999 0004 0000 CCCC 6666"
	$"0004 0000 CCCC 3333 0004 0000 CCCC 0000"
	$"0000 0000 9999 FFFF 0004 0000 9999 CCCC"
	$"0004 0000 9999 9999 0004 0000 9999 6666"
	$"0004 0000 9999 3333 0004 0000 9999 0000"
	$"0004 0000 6666 FFFF 0004 0000 6666 CCCC"
	$"0004 0000 6666 9999 0004 0000 6666 6666"
	$"0004 0000 6666 3333 0004 0000 6666 0000"
	$"0004 0000 3333 FFFF 0004 0000 3333 CCCC"
	$"0004 0000 3333 9999 0004 0000 3333 6666"
	$"0004 0000 3333 3333 0004 0000 3333 0000"
	$"0004 0000 0000 FFFF 0004 0000 0000 CCCC"
	$"0004 0000 0000 9999 0004 0000 0000 6666"
	$"0004 0000 0000 3333 0004 EEEE 0000 0000"
	$"0000 DDDD 0000 0000 0004 BBBB 0000 0000"
	$"0004 AAAA 0000 0000 0004 8888 0000 0000"
	$"0004 7777 0000 0000 0004 5555 0000 0000"
	$"0004 4444 0000 0000 0004 2222 0000 0000"
	$"0004 1111 0000 0000 0004 0000 EEEE 0000"
	$"0004 0000 DDDD 0000 0000 0000 BBBB 0000"
	$"0004 0000 AAAA 0000 0004 0000 8888 0000"
	$"0004 0000 7777 0000 0004 0000 5555 0000"
	$"0004 0000 4444 0000 0004 0000 2222 0000"
	$"0004 0000 1111 0000 0004 0000 0000 EEEE"
	$"0000 0000 0000 DDDD 0004 0000 0000 BBBB"
	$"0004 0000 0000 AAAA 0004 0000 0000 8888"
	$"0004 0000 0000 7777 0004 0000 0000 5555"
	$"0004 0000 0000 4444 0004 0000 0000 2222"
	$"0004 0000 0000 1111 0000 EEEE EEEE EEEE"
	$"0000 DDDD DDDD DDDD 0000 BBBB BBBB BBBB"
	$"0000 AAAA AAAA AAAA 0000 8888 8888 8888"
	$"0000 7777 7777 7777 0000 5555 5555 5555"
	$"0000 4444 4444 4444 0000 2222 2222 2222"
	$"0000 1111 1111 1111 0000 0000 0000 0000"
	$"0000 0000 0020 0020 0007 001E 0027 003E"
	$"0001 06FE 00E7 FFFE 0006 FD00 E9FF FD00"
	$"06FC 00EA FFFD 0006 FB00 ECFF FC00 06EB"
	$"00FC FFFC 0006 EC00 FCFF FB00 06EC 00FC"
	$"FFFB 0006 ED00 FCFF FA00 06ED 00FC FFFA"
	$"0006 EE00 FCFF F900 06EE 00FC FFF9 0006"
	$"EF00 FCFF F800 0BFA 0001 FFFF F800 FCFF"
	$"F800 0DFB 0003 FF05 05FF FA00 FCFF F700"
	$"0EFC 0000 FFFD 0500 FFFB 00FC FFF7 000E"
	$"FD00 06FF 0505 FFFF 0505 F1FF FC00 0FFD"
	$"0005 FF05 FF00 00FF F005 00FF FD00 10FD"
	$"0006 FF05 FF00 00FF 05F1 1600 FFFD 0013"
	$"FD00 06FF 1605 FFFF 0516 F5FF 0305 FF05"
	$"FFFC 0017 FC00 05FF 1605 0516 FFFE 00FC"
	$"FFFE 0004 FF16 FF16 FFFC 0013 FB00 03FF"
	$"1616 FFFD 00FC FFFD 0002 FF00 FFFB 000B"
	$"FA00 01FF FFFD 00FC FFF3 0006 F400 FCFF"
	$"F300 06F5 00FC FFF2 0006 F500 FCFF F200"
	$"06F6 00FC FFF1 0006 F600 FCFF F100 06F7"
	$"00FC FFF0 0006 F700 FCFF F000 06F8 00FC"
	$"FFEF 0006 F800 FCFF EF00 06F9 00FC FFEE"
	$"0000 00FF"
};

