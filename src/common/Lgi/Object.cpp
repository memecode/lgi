#include "lgi/common/Gdc2.h"
#include "lgi/common/LgiCommon.h"
#include "lgi/common/LgiString.h"

//////////////////////////////////////////////////////////////////////////
LBase::LBase()
{
	_Name8 = 0;
	_Name16 = 0;
}

LBase::~LBase()
{
	DeleteArray(_Name8);
	DeleteArray(_Name16);
}

const char *LBase::Name()
{
	if (!_Name8 && _Name16)
	{
		_Name8 = WideToUtf8(_Name16);
	}

	return _Name8;
}

bool LBase::Name(const char *n)
{
	if (n == _Name8) return true;

	DeleteArray(_Name8);
	DeleteArray(_Name16);
	_Name8 = NewStr(n);

	return _Name8 != 0;
}

const char16 *LBase::NameW()
{
	if (!_Name16 && _Name8)
	{
		_Name16 = Utf8ToWide(_Name8);
	}

	return _Name16;
}

bool LBase::NameW(const char16 *n)
{
	if (n == _Name16) return true;

	DeleteArray(_Name8);
	DeleteArray(_Name16);
	_Name16 = NewStrW(n);

	return _Name16 != 0;
}

