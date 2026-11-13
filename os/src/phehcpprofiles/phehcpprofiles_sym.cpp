// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehcpprofiles_sym.cpp -- the Symbian side of the Profiles applet: the profile names and the active profile through the Profiles engine
// wrapper (ProfileEngine.dll, ProEngFactory / MProEngEngine), the same calls the Symbian Profiles application and the power key menu make.
// The interfaces are declared here in the order of the public profiles_engine_wrapper_api headers (MProEngEngine.h, MProEngProfileNameArray.h,
// ProEngFactory.h), which the pdk does not export.
#include <e32base.h>
#include <bamdesca.h>
#include <string.h>

class MProEngProfile;
class MProEngProfileNameArrayExt;
class MProEngChangeObserverExt;

class MProEngProfileNameArray : public MDesCArray
	{
public:
	virtual TInt ProfileId(TInt aIndex) const = 0;
	virtual TInt FindById(TInt aId) const = 0;
	virtual TInt FindByName(const TDesC& aProfileName) const = 0;
	virtual MProEngProfileNameArrayExt* Extension() { return NULL; }
	};

class MProEngEngine
	{
public:
	virtual ~MProEngEngine() {}
	virtual void Release() = 0;
	virtual MProEngProfile* ActiveProfileLC() = 0;
	virtual MProEngProfile* ActiveProfileL() = 0;
	virtual TInt ActiveProfileId() = 0;
	virtual MProEngProfileNameArray* ProfileNameArrayLC() = 0;
	virtual void SetActiveProfileL(TInt aId) = 0;
	virtual MProEngProfile* ProfileLC(TInt aId) = 0;
	virtual MProEngProfile* ProfileL(TInt aId) = 0;
	virtual MProEngChangeObserverExt* Extension() { return NULL; }
	};

class ProEngFactory
	{
public:
	IMPORT_C static MProEngEngine* NewEngineL();
	};


static void ListL(int* aIds, char (*aNames)[48], int aMax, int* aCount, int* aActive)
	{
	MProEngEngine* e = ProEngFactory::NewEngineL();
	CleanupReleasePushL(*e);
	*aActive = e->ActiveProfileId();
	MProEngProfileNameArray* a = e->ProfileNameArrayLC();
	TInt n = a->MdcaCount();
	if (n > aMax) n = aMax;
	for (TInt i = 0; i < n; i++)
		{
		TPtrC s = a->MdcaPoint(i);
		TInt o = 0;
		for (TInt k = 0; k < s.Length() && o < 47; k++)
			aNames[i][o++] = s[k] < 0x80 ? (char)s[k] : '?';
		aNames[i][o] = 0;
		aIds[i] = a->ProfileId(i);
		}
	*aCount = n;
	CleanupStack::PopAndDestroy();   // a (ProfileNameArrayLC pushed it)
	CleanupStack::PopAndDestroy(e);
	}

// up to aMax profiles: their ids and names (ASCII), the active id; 0 or a Symbian error
extern "C" int phe_pf_list(int* aIds, char (*aNames)[48], int aMax, int* aCount, int* aActive)
	{
	*aCount = 0; *aActive = -1;
	TRAPD(err, ListL(aIds, aNames, aMax, aCount, aActive));
	return err;
	}

static void SetL(int aId)
	{
	MProEngEngine* e = ProEngFactory::NewEngineL();
	CleanupReleasePushL(*e);
	e->SetActiveProfileL(aId);
	CleanupStack::PopAndDestroy(e);
	}

extern "C" int phe_pf_set_active(int aId)
	{
	TRAPD(err, SetL(aId));
	return err;
	}
