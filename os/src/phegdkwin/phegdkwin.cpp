// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phegdkwin.cpp -- the gdk-symbian demo application: a real GdkWindow (phegdk.dll = GDK 2.24.33 core + the Symbian backend) showing Pango text, a gdk-pixbuf PNG, GDK raster
// operations and live key / tap events. All of the work is in phegdkwin_demo.c, built from the public GDK API (hildon-port.md 7.15).
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_gdk_demo_main(void);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	setenv("FONTCONFIG_FILE", "z:/resource/phegdkwin/fonts.conf", 1);
	setenv("PHEGDK_TRACE_PRESENT", "1", 1);
	TInt rc = 0;
	TRAPD(err, rc = phe_gdk_demo_main());      // Symbian calls made on behalf of GDK (bitmap and window server objects) need a trap level
	RDebug::Printf("[phegdkwin] exit rc=%d err=%d", rc, err);
	delete cleanup;
	return rc;
	}
