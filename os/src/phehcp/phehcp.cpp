// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehcp.cpp -- entry point of the Hildon control panel (menu caption "Settings (Hildon)"): hildon-control-panel (maemo-leste, LGPL-2.1, compiled unmodified)
// over libhildon, with its applets loaded as plug-in DLLs. The C side is the prebuilt archive libphehcp.a (recipes/build_hcp_x32.sh).
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_hcp_start(void);
extern "C" const char* phe_hcp_res(const char* aRel);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	setenv("FONTCONFIG_FILE", phe_hcp_res("resource/phehcp/fonts.conf"), 1);
	TInt rc = 0;
	TRAPD(err, rc = phe_hcp_start());      // Symbian calls made on behalf of GDK need a trap level
	RDebug::Printf("[phehcp] exit rc=%d err=%d", rc, err);
	delete cleanup;
	return rc;
	}
