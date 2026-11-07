// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehildonwin.cpp -- the the libhildon demo application: a HildonProgram with a HildonWindow, app menu, pannable area and touch widgets, on phehildon.dll over GTK 2.24 and the Symbian GDK backend (hildon-port.md 7.18)
// All of the work is in phehildonwin_demo.c, built from the public libhildon API.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_hildon_demo_main(void);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	setenv("FONTCONFIG_FILE", "z:/resource/phehildonwin/fonts.conf", 1);
	setenv("PHEGDK_TRACE_PRESENT", "1", 1);
	TInt rc = 0;
	TRAPD(err, rc = phe_hildon_demo_main());      // Symbian calls made on behalf of GDK (bitmap and window server objects) need a trap level
	RDebug::Printf("[phehildonwin] exit rc=%d err=%d", rc, err);
	delete cleanup;
	return rc;
	}
