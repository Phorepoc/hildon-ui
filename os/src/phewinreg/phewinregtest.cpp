// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phewinregtest.cpp -- the window registry's self test (hildon-port.md C4): an "application" session registers a window
// and a transient dialog, publishes a real CFbsBitmap as the window's content; a "frame" session waits for the changes,
// lists the windows, duplicates the bitmap by its handle (what the compositor's import does) and reads a pixel back; then
// the application session closes and its windows must go. Prints "[phewinregtest] PASS" or the first failure.
#include <e32base.h>
#include <e32debug.h>
#include <fbs.h>
#include "phewinreg.h"

#define CHECK(c) do { if (!(c)) { RDebug::Printf("[phewinregtest] FAIL line %d: %s", __LINE__, #c); return KErrGeneral; } } while (0)

static TInt ListAll(RPheWinReg& aReg, TPheWinInfo* aOut, TInt aMax)
	{
	TPtr8 p((TUint8*)aOut, 0, aMax * sizeof(TPheWinInfo));
	return aReg.List(p);
	}

static TInt Run()
	{
	CHECK(RFbsSession::Connect() == KErrNone);
	RPheWinReg app, frame;
	CHECK(app.Connect() == KErrNone);
	CHECK(frame.Connect() == KErrNone);
	TRequestStatus st, wst;
	app.Watchers(-1, wst);
	User::WaitForRequest(wst);
	CHECK(wst.Int() == 0);										// no subscriptions yet
	frame.Notify(0, st);

	TPheWinInfo w;
	w.iType = EPheWinNormal;
	w.iGeometry = TRect(TPoint(0, 0), TSize(360, 600));
	w.iTitle = _L("Notes");
	CHECK(app.Register(w) == KErrNone && w.iId > 0);
	User::WaitForRequest(st);
	app.Watchers(0, wst);										// the owner waits for a subscription
	CHECK(app.Subscribe(w.iId, EPheWinWatch) == KErrArgument);	// not to its own window
	CHECK(frame.Subscribe(w.iId + 100, EPheWinWatch) == KErrNotFound);
	CHECK(frame.Subscribe(w.iId, EPheWinComposite) == KErrNone);
	User::WaitForRequest(wst);
	CHECK(wst.Int() == 1);										// it came: publish a full frame now
	CHECK(st.Int() > 0);
	TUint seen = (TUint)st.Int();

	TPheWinInfo d = w;
	d.iId = 0;
	d.iType = EPheWinDialog;
	d.iTransientFor = w.iId;
	d.iState = EPheWinModal;
	d.iGeometry = TRect(TPoint(20, 200), TSize(320, 160));
	d.iTitle = _L("Delete note?");
	CHECK(app.Register(d) == KErrNone && d.iId != w.iId);

	CFbsBitmap* bmp = new CFbsBitmap;
	CHECK(bmp && bmp->Create(TSize(360, 600), EColor16MU) == KErrNone);
	bmp->LockHeap();
	TUint32* px = (TUint32*)bmp->DataAddress();
	px[0] = 0xFF336699u;
	bmp->UnlockHeap();
	CHECK(app.Content(w.iId, bmp->Handle(), TRect(0, 0, 360, 600)) == 1);	// one watcher: the frame session
	CHECK(frame.Content(w.iId, bmp->Handle(), TRect()) == KErrNotFound);	// only the owner publishes a window's content

	// the frame side: a change since `seen`, then the list
	frame.Notify(seen, st);
	User::WaitForRequest(st);
	CHECK((TUint)st.Int() != seen);
	TPheWinInfo list[4];
	CHECK(ListAll(frame, list, 1) == 2);								// too small: the count needed
	CHECK(ListAll(frame, list, 4) == 2);
	CHECK(list[0].iId == w.iId && list[0].iBitmap == bmp->Handle() && list[0].iContentSerial == 1 && list[0].iOwner == (TUint)RProcess().Id().Id());
	CHECK(list[0].iSubscribers == 1 && list[0].iComposited == 1 && list[1].iSubscribers == 0);	// per window: the dialog has none
	app.Watchers(1, wst);
	CHECK(frame.Subscribe(w.iId, EPheWinWatch) == KErrNone);				// composite -> watch only
	User::WaitForRequest(wst);
	CHECK(wst.Int() > 1);
	CHECK(ListAll(frame, list, 4) == 2 && list[0].iSubscribers == 1 && list[0].iComposited == 0);
	CHECK(list[1].iType == EPheWinDialog && list[1].iTransientFor == w.iId && (list[1].iState & EPheWinModal) && list[1].iTitle == _L("Delete note?"));
	CFbsBitmap* imported = new CFbsBitmap;
	CHECK(imported && imported->Duplicate(list[0].iBitmap) == KErrNone);
	imported->LockHeap();
	const TUint32 back = ((TUint32*)imported->DataAddress())[0];
	imported->UnlockHeap();
	CHECK(back == 0xFF336699u && imported->SizeInPixels() == TSize(360, 600));
	delete imported;

	// metadata update, then the application goes away: its windows with it
	d.iTitle = _L("Delete 1 note?");
	CHECK(app.Update(d) == KErrNone);
	frame.Notify(0, st);
	app.Close();
	User::WaitForRequest(st);
	CHECK(st.Int() > 0);
	CHECK(ListAll(frame, list, 4) == 0);
	frame.Close();
	delete bmp;
	RFbsSession::Disconnect();
	RDebug::Printf("[phewinregtest] PASS: register, per-window subscribe (watch / composite), transient dialog, content handle + duplicate, notify, list, cleanup on close");
	return KErrNone;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	const TInt r = Run();
	delete cleanup;
	return r;
	}
