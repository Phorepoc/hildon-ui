// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phewinreg.cpp -- the window registry server (hildon-port.md C4; the protocol is phewinreg.h). One CServer2, the window
// records in registration order, one pending change notification per session. Content updates only record the owner's
// FBSERV bitmap handle and damage: the frame process duplicates the bitmap (CFbsBitmap::Duplicate) when it imports it.
// Exits when the last session closes and no window is left, as transient servers do.
#include <e32base.h>
#include <e32debug.h>
#include "phewinreg.h"

class CRegSession;

class CRegServer : public CServer2
	{
public:
	CRegServer() : CServer2(EPriorityStandard, ESharableSessions) {}
	~CRegServer() { iWins.Close(); }
	CSession2* NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const;
	void Changed();
	void SessionOpened() { ++iSessions; }
	void SessionClosed(CRegSession* aSession);
	void WatchersChanged();
	TInt Find(TInt aId) const
		{
		for (TInt i = 0; i < iWins.Count(); ++i)
			if (iWins[i].iInfo.iId == aId)
				return i;
		return KErrNotFound;
		}
	struct TWin { TPheWinInfo iInfo; const CRegSession* iSession; };
	RArray<TWin> iWins;
	TInt iNextId = 1;
	TUint iSerial = 1;
	TInt iSessions = 0;
	TInt iWatchers = 0;			// sessions that asked for change notification (the frame process): content is worth publishing
	RPointerArray<CRegSession> iWaiters;
	};

class CRegSession : public CSession2
	{
public:
	~CRegSession() { Server().SessionClosed(this); }
	void CreateL() { Server().SessionOpened(); }
	void ServiceL(const RMessage2& aMessage);
	void Complete(TUint aSerial)
		{
		if (!iNotify.IsNull() && aSerial != iSeen)
			iNotify.Complete((TInt)aSerial);
		}
	RMessagePtr2 iNotify;
	TUint iSeen;
	TBool iWatcher = EFalse;
	RMessagePtr2 iWatch;			// an owner waiting for the watcher count to change
	TInt iWatchSeen;
private:
	CRegServer& Server() const { return *static_cast<CRegServer*>(const_cast<CServer2*>(CSession2::Server())); }
	TInt Owns(TInt aId) const
		{
		const TInt i = Server().Find(aId);
		return i >= 0 && Server().iWins[i].iSession == this ? i : KErrNotFound;
		}
	};

CSession2* CRegServer::NewSessionL(const TVersion& aVersion, const RMessage2&) const
	{
	if (!User::QueryVersionSupported(TVersion(1, 0, 0), aVersion))
		User::Leave(KErrNotSupported);
	return new (ELeave) CRegSession;
	}

void CRegServer::Changed()
	{
	++iSerial;
	for (TInt i = iWaiters.Count() - 1; i >= 0; --i)
		{
		CRegSession* s = iWaiters[i];
		s->Complete(iSerial);
		if (s->iNotify.IsNull())
			iWaiters.Remove(i);
		}
	}

// the watcher count changed: every owner waiting on it hears the new count
void CRegServer::WatchersChanged()
	{
	iSessionIter.SetToFirst();
	for (CSession2* s; (s = iSessionIter++) != NULL; )
		{
		CRegSession* r = static_cast<CRegSession*>(s);
		if (!r->iWatch.IsNull() && r->iWatchSeen != iWatchers)
			r->iWatch.Complete(iWatchers);
		}
	RDebug::Printf("[phewinreg] %d watching", iWatchers);
	}

void CRegServer::SessionClosed(CRegSession* aSession)
	{
	TBool removed = EFalse;
	for (TInt i = iWins.Count() - 1; i >= 0; --i)
		if (iWins[i].iSession == aSession)
			{
			RDebug::Printf("[phewinreg] window %d gone with its session", iWins[i].iInfo.iId);
			iWins.Remove(i);
			removed = ETrue;
			}
	aSession->iWatch = RMessagePtr2();		// (the kernel completes a closing session's messages)
	if (aSession->iWatcher)
		{
		--iWatchers;
		WatchersChanged();
		}
	const TInt w = iWaiters.Find(aSession);
	if (w >= 0)
		iWaiters.Remove(w);
	if (removed)
		Changed();
	if (--iSessions == 0 && iWins.Count() == 0)
		CActiveScheduler::Stop();
	}

void CRegSession::ServiceL(const RMessage2& aMessage)
	{
	CRegServer& srv = Server();
	switch (aMessage.Function())
		{
	case EPheWinRegister:
	case EPheWinUpdate:
		{
		TPheWinInfoPckg p;
		aMessage.ReadL(0, p);
		TPheWinInfo& in = p();
		if (aMessage.Function() == EPheWinRegister)
			{
			CRegServer::TWin w;
			w.iInfo = in;
			w.iInfo.iId = srv.iNextId++;
			w.iInfo.iBitmap = 0;
			w.iInfo.iContentSerial = 0;
			w.iInfo.iDamage = TRect();
			RThread client;
			RProcess proc;
			w.iInfo.iOwner = 0;
			if (aMessage.Client(client) == KErrNone)
				{
				if (client.Process(proc) == KErrNone)
					{
					w.iInfo.iOwner = (TUint)proc.Id().Id();
					proc.Close();
					}
				client.Close();
				}
			w.iSession = this;
			srv.iWins.AppendL(w);
			RDebug::Print(_L("[phewinreg] window %d registered: type %d, %dx%d, '%S'"), w.iInfo.iId, w.iInfo.iType,
				w.iInfo.iGeometry.Width(), w.iInfo.iGeometry.Height(), &w.iInfo.iTitle);
			aMessage.Complete(w.iInfo.iId);
			}
		else
			{
			const TInt i = Owns(in.iId);
			if (i < 0) { aMessage.Complete(KErrNotFound); return; }
			TPheWinInfo& cur = srv.iWins[i].iInfo;
			cur.iType = in.iType;
			cur.iTransientFor = in.iTransientFor;
			cur.iGeometry = in.iGeometry;
			cur.iState = in.iState;
			cur.iTitle = in.iTitle;
			aMessage.Complete(KErrNone);
			}
		srv.Changed();
		return;
		}
	case EPheWinContent:
		{
		const TInt i = Owns(aMessage.Int0());
		if (i < 0) { aMessage.Complete(KErrNotFound); return; }
		TPckgBuf<TRect> d;
		aMessage.ReadL(2, d);
		TPheWinInfo& cur = srv.iWins[i].iInfo;
		cur.iBitmap = aMessage.Int1();
		cur.iDamage = d();
		++cur.iContentSerial;
		aMessage.Complete(srv.iWatchers);		// how many watch
		srv.Changed();
		return;
		}
	case EPheWinRemove:
		{
		const TInt i = Owns(aMessage.Int0());
		if (i < 0) { aMessage.Complete(KErrNotFound); return; }
		srv.iWins.Remove(i);
		aMessage.Complete(KErrNone);
		srv.Changed();
		return;
		}
	case EPheWinList:
		{
		const TInt n = srv.iWins.Count();
		const TInt room = aMessage.GetDesMaxLengthL(0) / (TInt)sizeof(TPheWinInfo);
		if (room < n) { aMessage.Complete(n); return; }
		HBufC8* buf = HBufC8::NewLC(n * sizeof(TPheWinInfo) + 1);
		TPtr8 b = buf->Des();
		for (TInt i = 0; i < n; ++i)
			b.Append((const TUint8*)&srv.iWins[i].iInfo, sizeof(TPheWinInfo));
		aMessage.WriteL(0, b);
		CleanupStack::PopAndDestroy(buf);
		aMessage.Complete(n);
		return;
		}
	case EPheWinNotify:
		if (!iNotify.IsNull()) { aMessage.Complete(KErrInUse); return; }
		if (!iWatcher) { iWatcher = ETrue; ++srv.iWatchers; srv.WatchersChanged(); }
		iSeen = (TUint)aMessage.Int0();
		if (iSeen && iSeen != srv.iSerial) { aMessage.Complete((TInt)srv.iSerial); return; }	// it changed since the caller looked
		iNotify = aMessage;
		srv.iWaiters.AppendL(this);
		return;
	case EPheWinNotifyCancel:
		if (!iNotify.IsNull())
			iNotify.Complete(KErrCancel);
		{
		const TInt w = srv.iWaiters.Find(this);
		if (w >= 0) srv.iWaiters.Remove(w);
		}
		aMessage.Complete(KErrNone);
		return;
	case EPheWinWatchers:
		if (!iWatch.IsNull()) { aMessage.Complete(KErrInUse); return; }
		iWatchSeen = aMessage.Int0();
		if (iWatchSeen != srv.iWatchers) { aMessage.Complete(srv.iWatchers); return; }
		iWatch = aMessage;
		return;
	case EPheWinWatchersCancel:
		if (!iWatch.IsNull())
			iWatch.Complete(KErrCancel);
		aMessage.Complete(KErrNone);
		return;
	default:
		aMessage.Panic(KPheWinRegServer, 1);
		}
	}

static void RunL()
	{
	CActiveScheduler* as = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(as);
	CActiveScheduler::Install(as);
	CRegServer* srv = new (ELeave) CRegServer;
	CleanupStack::PushL(srv);
	const TInt r = srv->Start(KPheWinRegServer);
	if (r == KErrAlreadyExists) { RProcess::Rendezvous(r); CleanupStack::PopAndDestroy(2); return; }
	User::LeaveIfError(r);
	User::RenameThread(KPheWinRegServer);
	RProcess::Rendezvous(KErrNone);
	RDebug::Printf("[phewinreg] up");
	CActiveScheduler::Start();
	RDebug::Printf("[phewinreg] last session gone: exit");
	CleanupStack::PopAndDestroy(2);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, RunL());
	if (err != KErrNone)
		RProcess::Rendezvous(err);
	delete cleanup;
	return err;
	}
