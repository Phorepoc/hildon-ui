// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehildondesktoptest.cpp -- the libhildondesktop test: heartbeat, shortcuts over GConf, a home plug-in item, the D-Bus connection (hildon-port.md 7.19)
// All of the work is in phehildondesktoptest_demo.c, in phehildondesktoptest_main.c.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_hildondesktop_test_main(void);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	setenv("FONTCONFIG_FILE", "z:/resource/phehildondesktoptest/fonts.conf", 1);
	setenv("PHEGDK_TRACE_PRESENT", "1", 1);
	TInt rc = 0;
	TRAPD(err, rc = phe_hildondesktop_test_main());      // Symbian calls made on behalf of GDK (bitmap and window server objects) need a trap level
	RDebug::Printf("[phehildondesktoptest] exit rc=%d err=%d", rc, err);
	delete cleanup;
	return rc;
	}
