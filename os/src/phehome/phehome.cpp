// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehome.cpp -- entry point of the Home process (phehome_main.c): Hildon's applet manager (from hildon-home) over libhildondesktop, with the home applets loaded as plug-in DLLs.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <stdlib.h>

extern "C" void phe_log(const char* aMsg);
extern "C" int phe_home_main(void);

extern "C" void phe_log(const char* aMsg) { RDebug::Printf("%s", aMsg); }

extern "C" int phehome_spawn(const char* aExe, const char* aArgs)
	{
	TBuf<64> exe;
	exe.Copy(TPtrC8((const TUint8*)aExe));
	TBuf<256> args;
	args.Copy(TPtrC8((const TUint8*)aArgs, Min((TInt)User::StringLength((const TUint8*)aArgs), 255)));
	RProcess p;
	TInt r = p.Create(exe, args);
	if (r == KErrNone)
		{
		p.Resume();
		p.Close();
		}
	return r;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	setenv("FONTCONFIG_FILE", "z:/resource/phehome/fonts.conf", 1);
	TInt rc = 0;
	TRAPD(err, rc = phe_home_main());
	RDebug::Printf("[phehome] exit rc=%d err=%d", rc, err);
	delete cleanup;
	return rc;
	}
