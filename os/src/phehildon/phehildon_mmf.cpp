// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehildon_mmf.cpp -- the audio backend of the libcanberra client API (phehildon_canberra.c): a sample file played through the platform's MMF (CMdaAudioPlayerUtility), as the rest of the OS plays
// sound. Every playback runs in its own thread with its own active scheduler (the MMF utility needs one, the GTK main thread has none), and ends when the sample has been played.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <mdaaudiosampleplayer.h>
#include <string.h>
#include <stdlib.h>

extern "C" {
typedef void (*PheAudioDone)(int aError, void* aUser);
int phe_audio_play_file(const char* aPath, PheAudioDone aDone, void* aUser);
}

class CPlay : public CBase, public MMdaAudioPlayerCallback
	{
public:
	TFileName iPath;
	PheAudioDone iDone;
	void* iUser;
	CMdaAudioPlayerUtility* iPlayer;
	TInt iError;
	void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds&)
		{
		RDebug::Printf("[phecanberra] sample opened, error %d", aError);
		if (aError != KErrNone) { Finish(aError); return; }
		iPlayer->Play();
		}
	void MapcPlayComplete(TInt aError) { Finish(aError); }
	void Finish(TInt aError) { iError = aError; CActiveScheduler::Stop(); }
	};

static TInt PlayThread(TAny* aArg)
	{
	CPlay* p = (CPlay*)aArg;
	CTrapCleanup* cleanup = CTrapCleanup::New();
	CActiveScheduler* sched = new CActiveScheduler;
	if (!cleanup || !sched) { p->iDone(KErrNoMemory, p->iUser); return KErrNoMemory; }
	CActiveScheduler::Install(sched);
	TRAPD(err, p->iPlayer = CMdaAudioPlayerUtility::NewFilePlayerL(p->iPath, *p));
	if (err != KErrNone) p->iError = err;
	else CActiveScheduler::Start();
	delete p->iPlayer;
	RDebug::Printf("[phecanberra] playback of the sample ended, error %d", p->iError);
	p->iDone(p->iError, p->iUser);
	delete p;
	delete sched;
	delete cleanup;
	return KErrNone;
	}

// aPath: a UTF-8 path with '/' or '\'; a path without a drive letter is looked up on Z: (the ROM, where samples are installed)
extern "C" int phe_audio_play_file(const char* aPath, PheAudioDone aDone, void* aUser)
	{
	CPlay* p = new CPlay;
	if (!p) return KErrNoMemory;
	TBuf8<256> utf8;
	utf8.Copy(TPtrC8((const TUint8*)aPath, Min((TInt)strlen(aPath), 255)));
	TBuf<256> wide;
	wide.Copy(utf8);
	for (TInt i = 0; i < wide.Length(); i++) if (wide[i] == '/') wide[i] = '\\';
	if (wide.Length() > 1 && wide[1] != ':') { TBuf<256> t(_L("z:")); if (wide[0] != '\\') t.Append('\\'); t.Append(wide); wide = t; }
	p->iPath.Copy(wide);
	p->iDone = aDone; p->iUser = aUser; p->iPlayer = 0; p->iError = KErrNone;
	RDebug::Printf("[phecanberra] playing %S", &p->iPath);
	RThread th;
	TInt r = th.Create(_L(""), PlayThread, 0x4000, 0x1000, 0x100000, p, EOwnerProcess);
	if (r != KErrNone) { delete p; return r; }
	th.Resume();
	th.Close();
	return KErrNone;
	}
