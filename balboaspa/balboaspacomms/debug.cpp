#include "stdafx.h"

#include "Debug.h"


void DumpHexData(
	const BYTE *pData,
	size_t iBytes)
{
	for (int i = 0; i < iBytes; i++)
	{
		BYTE c = pData[i];
		wprintf(L"%02x ", c);
	}
	wprintf(L"\n");
}

void DumpHexData(
	const CByteArray &array)
{
	DumpHexData(&array[0], array.size());
}

