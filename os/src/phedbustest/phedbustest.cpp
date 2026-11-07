// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phedbustest.cpp -- the D-Bus test: libdbus over the real transport (a TCP server on the loopback, a client, a method call and a signal)
// All of the work is in phedbustest_demo.c, in phedbustest_main.c.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_dbus_test_main(void);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TInt rc = 0;
	TRAPD(err, rc = phe_dbus_test_main());      // Symbian calls made on behalf of GDK (bitmap and window server objects) need a trap level
	RDebug::Printf("[phedbustest] exit rc=%d err=%d", rc, err);
	delete cleanup;
	return rc;
	}
