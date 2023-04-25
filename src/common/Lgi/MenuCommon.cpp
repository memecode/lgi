//
//  LMenuCommon.cpp
//  LgiCarbon
//
//  Created by Matthew Allen on 30/01/2018.
//
//

#include "lgi/common/Lgi.h"
#include "lgi/common/Menu.h"

bool LSubMenu::GetVariant(const char *Name, LVariant &Value, const char *Array)
{
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		case ObjLength:
			Value = Length();
			break;
		default:
			return false;
	}
	return true;
}

bool LSubMenu::SetVariant(const char *Name, LVariant &Value, const char *Array)
{
	/*
	LDomProperty p = LStringToDomProp(Name);
	switch (p)
	{
		default:
			return false;
	}
	*/
	return false;
}

bool LSubMenu::CallMethod(const char *MethodName, LScriptArguments &Args)
{
	LDomProperty Method = LStringToDomProp(MethodName);	
	switch (Method)
	{
		case ::AppendSeparator:
		{
			*Args.GetReturn() = AppendSeparator();
			break;
		}
		case ::AppendItem:
		{
			auto Str		= Args.StringAt(0);
			auto Id			= Args.Int32At(1, -1);
			auto Enabled	= Args.Int32At(2, true);
			auto Where		= Args.Int32At(3, -1);
			auto ShortCut	= Args.StringAt(4);

			*Args.GetReturn() = AppendItem(Str, Id, Enabled, Where, ShortCut);
			break;
		}
		default:
			return false;
	}
	return true;
}
