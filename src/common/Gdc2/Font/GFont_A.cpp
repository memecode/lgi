/*hdr
**	FILE:			GFont_A.cpp
**	AUTHOR:			Matthew Allen
**	DATE:			19/6/2002
**	DESCRIPTION:	8Bit Char Font Support
**
**	Copyright (C) 2002, Matthew Allen
**		fret@memecode.com
*/

//////////////////////////////////////////////////////////////////////
// Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lgi/common/Lgi.h"
#include "lgi/common/FontSelect.h"
#include "lgi/common/GdiLeak.h"

#ifdef FontChange
#undef FontChange
#endif

/////////////////////////////////////////////////////////////////////////////
/*
void LFont::Text(	GSurface *pDC,
					int x, int y,
					char *Str,
					int Len,
					LRect *r,
					int TabOrigin)
{
	COLOUR ForeCol = Fore();
	COLOUR BackCol = Back();
	bool Trans = Transparent();

	if (pDC AND
		Str AND
		IsValid())
	{
		if (Len < 0)
		{
			Len = LgiByteLen(Str, "utf-8");
		}

		if (Len > 0)
		{
			LDisplayString Ds(this, Str, Len, TabOrigin);
			Ds.Draw(pDC, x, y, r);
		}
		else if (r AND !Transparent())
		{
			pDC->Colour(Back(), 24);
			pDC->Rectangle(r);
		}
	}
}

void LFont::Size(int *x, int *y, char *Str, int Len, int Flags)
{
	if (x) *x = 0;
	if (y) *y = GetHeight();
	
	if (Str AND IsValid())
	{
		if (x)
		{
			if (Len < 0)
			{
				Len = strlen(Str);
			}
			LDisplayString Ds(this, Str, Len);
			*x = Ds.X();
		}
	}
}

int LFont::CharAt(int x, char *Str, int Len, int TabOffset)
{
	if (!Str || x <= 0)
	{
		return 0;
	}

	if (IsValid())
	{
		LDisplayString s(this, Str, Len, TabOffset);
		return s.CharAt(x);
	}

	return 0;
}

int LFont::X(char *Str, int Len)
{
	int x = 0;
	if (IsValid())
	{
		Size(&x, 0, Str, Len, 0);
	}
	return x;
}

int LFont::Y(char *Str, int Len)
{
	int y = 0;
	if (IsValid())
	{
		Size(0, &y, Str, Len, 0);
	}
	return y;
}

*/