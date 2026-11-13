// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// phehomestatus_sym.cpp -- the Symbian side of the device status home widget: free RAM (HAL EMemoryRAM / EMemoryRAMFree) and the C: drive (RFs::Volume).
#include <e32std.h>
#include <f32file.h>
#include <hal.h>

// in MB; -1 = not known
extern "C" void phe_st_now(int* aRamTotal, int* aRamFree, int* aDiskTotal, int* aDiskFree)
	{
	TInt t = 0, f = 0;
	*aRamTotal = HAL::Get(HALData::EMemoryRAM, t) == KErrNone ? t >> 20 : -1;
	*aRamFree = HAL::Get(HALData::EMemoryRAMFree, f) == KErrNone ? f >> 20 : -1;
	*aDiskTotal = *aDiskFree = -1;
	RFs fs;
	if (fs.Connect() != KErrNone) return;
	TVolumeInfo v;
	if (fs.Volume(v, EDriveC) == KErrNone)
		{
		*aDiskTotal = (int)(v.iSize >> 20);
		*aDiskFree = (int)(v.iFree >> 20);
		}
	fs.Close();
	}
