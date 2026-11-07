// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phegtkwin.cpp -- the GTK 2.24.33 demo application: a real GtkWindow with widgets on phegtk.dll over the Symbian GDK backend (hildon-port.md 7.17)
// All of the work is in phegtkwin_demo.c, built from the public GTK API.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_gtk_demo_main(void);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	setenv("FONTCONFIG_FILE", "z:/resource/phegtkwin/fonts.conf", 1);
	setenv("PHEGDK_TRACE_PRESENT", "1", 1);
	TInt rc = 0;
	TRAPD(err, rc = phe_gtk_demo_main());      // Symbian calls made on behalf of GDK (bitmap and window server objects) need a trap level
	RDebug::Printf("[phegtkwin] exit rc=%d err=%d", rc, err);
	delete cleanup;
	return rc;
	}
