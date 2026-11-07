// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phedbustest.cpp -- D-Bus test: libdbus over the real transport (a TCP server on the loopback, a client, a method call and a signal), in phedbustest_main.c. This wrapper brings the network
// up the way an application on this system does: an RConnection started on the default access point, kept open while the test runs (a TCP/IP stack with no connection refuses to bind).
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <es_sock.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_dbus_test_main(void);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	RSocketServ ss;
	RConnection conn;
	TInt r = ss.Connect();
	if (r == KErrNone) r = conn.Open(ss);
	if (r == KErrNone) r = conn.Start();
	RDebug::Printf("[phedbustest] network connection started: %d", r);
	TInt rc = 0;
	TRAPD(err, rc = phe_dbus_test_main());
	RDebug::Printf("[phedbustest] exit rc=%d err=%d", rc, err);
	conn.Close();
	ss.Close();
	delete cleanup;
	return rc;
	}
