// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehcpabout_sym.cpp -- the Symbian side of the About device applet: the software version (z:\resource\versions\sw.txt, the file the
// Symbian "About" / *#0000# reads), the UI language (User::Language), RAM (HAL EMemoryRAM / EMemoryRAMFree) and the C: drive (RFs::Volume).
#include <e32std.h>
#include <f32file.h>
#include <hal.h>
#include <string.h>

// the first line of sw.txt as UTF-8 into aBuf (UTF-16 with a BOM, as the platform writes it, or 8-bit); 0 or a Symbian error
extern "C" int phe_ab_sw_version(char* aBuf, int aLen)
	{
	if (aLen < 1) return KErrArgument;
	aBuf[0] = 0;
	RFs fs;
	TInt err = fs.Connect();
	if (err != KErrNone) return err;
	RFile f;
	err = f.Open(fs, _L("z:\\resource\\versions\\sw.txt"), EFileRead | EFileShareReadersOnly);
	if (err == KErrNone)
		{
		TBuf8<256> raw;
		err = f.Read(raw);
		f.Close();
		TInt o = 0;
		if (err == KErrNone && raw.Length() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE)
			{
			for (TInt i = 2; i + 1 < raw.Length() && o < aLen - 1; i += 2)
				{
				TUint c = raw[i] | (raw[i + 1] << 8);
				if (c == '\n' || c == '\r') break;
				aBuf[o++] = (c < 0x80) ? (char)c : '?';
				}
			}
		else if (err == KErrNone)
			{
			for (TInt i = 0; i < raw.Length() && o < aLen - 1; i++)
				{
				if (raw[i] == '\n' || raw[i] == '\r') break;
				aBuf[o++] = (char)raw[i];
				}
			}
		aBuf[o] = 0;
		}
	fs.Close();
	return err;
	}

extern "C" int phe_ab_language(void)
	{
	return User::Language();
	}

// in KiB; -1 = not known
extern "C" void phe_ab_ram(int* aTotalKb, int* aFreeKb)
	{
	TInt t = 0, f = 0;
	*aTotalKb = HAL::Get(HALData::EMemoryRAM, t) == KErrNone ? t / 1024 : -1;
	*aFreeKb = HAL::Get(HALData::EMemoryRAMFree, f) == KErrNone ? f / 1024 : -1;
	}

// a drive (0 = A: ... 2 = C:), in KiB; a Symbian error if it is not there
extern "C" int phe_ab_drive(int aDrive, int* aTotalKb, int* aFreeKb)
	{
	RFs fs;
	TInt err = fs.Connect();
	if (err != KErrNone) return err;
	TVolumeInfo v;
	err = fs.Volume(v, aDrive);
	if (err == KErrNone)
		{
		*aTotalKb = (int)(v.iSize / 1024);
		*aFreeKb = (int)(v.iFree / 1024);
		}
	fs.Close();
	return err;
	}
