// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehangtest.cpp -- a classic (Avkon) test app that hangs on request: the "not responding" path of a classic app shown as a
// window (hildon-port.md 7.51.20). Options > "Hang" busy-loops in the UI thread forever, so the app stops running its event
// loop while its thread stays alive. For scripted runs, a document name "hang<seconds>" (phescreenlaunch window
// phehangtest.exe hang30) hangs the same way after that many seconds.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <avkon.hrh>
#include "phehangtest.hrh"

const TUid KUidPheHangTest = { 0xE701573C };

class CHangView : public CCoeControl
	{
public:
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		SetRect(aRect);
		ActivateL();
		}
private:
	void Draw(const TRect&) const
		{
		CWindowGc& gc = SystemGc();
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(0x30, 0x20, 0x20));
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.DrawRect(Rect());
		gc.UseFont(iEikonEnv->NormalFont());
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(KRgbWhite);
		gc.DrawText(_L("Hang test: Options > Hang"), TPoint(10, 40));
		gc.DiscardFont();
		}
	};

class CHangAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL();
		iView = new (ELeave) CHangView;
		iView->ConstructL(ClientRect());
		AddToStackL(iView);
		}
	~CHangAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		delete iTimer;
		}
	TBool ProcessCommandParametersL(TApaCommand aCommand, TFileName& aDocumentName, const TDesC8& aTail)
		{
		// "hang<seconds>": hang after that many seconds (the document name only carries the request; no file is opened)
		TParsePtrC parse(aDocumentName);
		TPtrC name = parse.Name();
		if (name.Left(4).CompareF(_L("hang")) == 0)
			{
			TLex lex(name.Mid(4));
			TInt secs = 0;
			lex.Val(secs);
			iTimer = CPeriodic::NewL(CActive::EPriorityStandard);
			iTimer->Start(Max(secs, 1) * 1000000, 1000000, TCallBack(HangCb, this));
			RDebug::Printf("[phehangtest] will hang in %d s", Max(secs, 1));
			aDocumentName.Zero();
			}
		return CAknAppUi::ProcessCommandParametersL(aCommand, aDocumentName, aTail);
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheHangCmdHang:
			Hang();
			break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			RDebug::Printf("[phehangtest] exit command %d", aCommand);
			Exit();
			break;
			}
		}
private:
	static TInt HangCb(TAny* aSelf)
		{
		static_cast<CHangAppUi*>(aSelf)->Hang();
		return 0;
		}
	void Hang()
		{
		RDebug::Printf("[phehangtest] hanging: busy loop in the UI thread");
		volatile TUint spin = 0;
		for (;;)
			++spin;
		}
	CHangView* iView;
	CPeriodic* iTimer;
	};

class CHangDocument : public CAknDocument
	{
public:
	CHangDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CHangAppUi; }
	};

class CHangApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KUidPheHangTest; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CHangDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CHangApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
