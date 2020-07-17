#include "Dialogs.r"

resource 'DLOG' (128) {
	{ 50, 100, 240, 420 },
	dBoxProc,
	visible,
	noGoAway,
	0,
	128,
	"",
	centerMainScreen
};

resource 'DITL' (128) {
	{
		{ 190-10-20, 320-10-80, 190-10, 320-10 },
		Button { enabled, "Connect" };

		{ 190-10-20-5, 320-10-80-5, 190-10+5, 320-10+5 },
		UserItem { enabled };

		{ 10, 10, 30, 310 },
		StaticText { enabled, "Address with port" };

		{ 35, 10, 51, 310 },
		EditText { enabled, "10.0.2.2:22" };

		{ 60, 10, 80, 310 },
		StaticText { enabled, "Username" };

		{ 85, 10, 101, 310 },
		EditText { enabled, "" };

		{ 110, 10, 130, 310 },
		StaticText { enabled, "Password" };

		{ 135, 10, 151, 310 },
		EditText { enabled, "" };
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
	1024 * 1024,
	1024 * 1024
};
