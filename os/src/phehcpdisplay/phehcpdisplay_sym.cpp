// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehcpdisplay_sym.cpp -- the Symbian side of the Display applet: the display light timeout, the central repository key the
// Symbian display settings change (KCRUidLightSettings 0x10200C8C, KDisplayLightsTimeout 6, seconds: HWRMLightDomainCRKeys.h).
#include <e32std.h>
#include <centralrepository.h>

static const TUid KPheLightSettings = {0x10200C8C};
static const TUint32 KPheDisplayLightsTimeout = 0x00000006;

extern "C" int phe_dp_get_timeout(void)
	{
	TInt v = -1;
	CRepository* r = NULL;
	TRAPD(err, r = CRepository::NewL(KPheLightSettings));
	if (err == KErrNone)
		{
		if (r->Get(KPheDisplayLightsTimeout, v) != KErrNone) v = -1;
		delete r;
		}
	return v;
	}

extern "C" int phe_dp_set_timeout(int aSeconds)
	{
	CRepository* r = NULL;
	TRAPD(err, r = CRepository::NewL(KPheLightSettings));
	if (err != KErrNone) return err;
	err = r->Set(KPheDisplayLightsTimeout, aSeconds);
	delete r;
	return err;
	}
