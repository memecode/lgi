#include "lgi/common/Lgi.h"
#include "FontPriv.h"

LTypeFace::LTypeFace()
{
	d = new LTypeFacePrivate;
}

LTypeFace::~LTypeFace()
{
	DeleteObj(d);
}

bool LTypeFace::operator ==(LTypeFace &t)
{
	if ((Face() == 0) ^ (t.Face() == 0))
		return false;
	if (Face() && t.Face() && stricmp(Face(), t.Face()) != 0)
		return false;
	if (Size() != t.Size())
		return false;
	if (GetWeight() != t.GetWeight())
		return false;
	if (Italic() != t.Italic())
		return false;
	if (Underline() != t.Underline())
		return false;
	return true;
}

// set
void LTypeFace::Face(const char *s)
{
	if (s &&
		s != d->_Face &&
		stricmp(s, d->_Face?d->_Face:(char*)"") != 0)
	{
		DeleteArray(d->_Face);
		d->_Face = NewStr(s);
		LgiAssert(d->_Face != NULL);
		_OnPropChange(true);
	}
}

void LTypeFace::Size(LCss::Len s)
{
	if (d->_Size != s)
	{
		d->_Size = s;
		_OnPropChange(true);
	}
}

void LTypeFace::PointSize(int i)
{
	Size(LCss::Len(LCss::LenPt, (float)i));
}

void LTypeFace::TabSize(int i)
{
	d->_TabSize = MAX(i, 8);
	_OnPropChange(false);
}

void LTypeFace::Quality(int i)
{
	if (d->_Quality != i)
	{
		d->_Quality = i;
		_OnPropChange(true);
	}
}

LColour LTypeFace::WhitespaceColour()
{
	if (d->WhiteSpace.IsValid())
		return d->WhiteSpace;
	
	return d->_Back.Mix(d->_Fore, LGI_WHITESPACE_WEIGHT);
}

void LTypeFace::WhitespaceColour(LColour c)
{
	d->WhiteSpace = c;
	_OnPropChange(false);
}

void LTypeFace::Fore(LSystemColour c)
{
	d->_Fore = LColour(c);
	_OnPropChange(false);
}

void LTypeFace::Fore(LColour c)
{
	d->_Fore = c;
	_OnPropChange(false);
}

void LTypeFace::Back(LSystemColour c)
{
	d->_Back = LColour(c);
	_OnPropChange(false);
}

void LTypeFace::Back(LColour c)
{
	d->_Back = c;
	_OnPropChange(false);
}

void LTypeFace::SetWeight(int i)
{
	if (d->_Weight != i)
	{
		d->_Weight = i;
		_OnPropChange(true);
	}
}

void LTypeFace::Italic(bool i)
{
	if (d->_Italic != i)
	{
		d->_Italic = i;
		_OnPropChange(true);
	}
}

void LTypeFace::Underline(bool i)
{
	if (d->_Underline != i)
	{
		d->_Underline = i;
		_OnPropChange(true);
	}
}

void LTypeFace::Transparent(bool i)
{
	d->_Transparent = i;
	_OnPropChange(false);
}

void LTypeFace::Colour(LSystemColour Fore, LSystemColour Back)
{
	d->_Fore = LColour(Fore);
	d->_Back = LColour(Back);
	_OnPropChange(false);
}

void LTypeFace::Colour(LColour Fore, LColour Back)
{
	LgiAssert(Fore.IsValid());
	d->_Fore = Fore;
	d->_Back = Back;
	// Transparent(Back.Transparent());
	_OnPropChange(false);
}

void LTypeFace::SubGlyphs(bool i)
{
	if (!i || LFontSystem::Inst()->GetGlyphSubSupport())
	{
		d->_SubGlyphs = i;
		_OnPropChange(false);
	}
}

////////////////////////
// get
char *LTypeFace::Face()
{
	return d->_Face;
}

LCss::Len LTypeFace::Size()
{
	return d->_Size;
}

int LTypeFace::PointSize()
{
	if (d->_Size.Type == LCss::LenPt)
		return (int)d->_Size.Value;
	
	if (d->_Size.Type == LCss::LenPx)
		return (int) (d->_Size.Value * 72 / LgiScreenDpi());

	LgiAssert(!"What now?");
	return 0;
}

int LTypeFace::TabSize()
{
	return d->_TabSize;
}

int LTypeFace::Quality()
{
	return d->_Quality;
}

LColour LTypeFace::Fore()
{
	return d->_Fore;
}

LColour LTypeFace::Back()
{
	return d->_Back;
}

int LTypeFace::GetWeight()
{
	return d->_Weight;
}

bool LTypeFace::Italic()
{
	return d->_Italic;
}

bool LTypeFace::Underline()
{
	return d->_Underline;
}

bool LTypeFace::Transparent()
{
	return d->_Transparent;
}

bool LTypeFace::SubGlyphs()
{
	return d->_SubGlyphs;
}

double LTypeFace::Ascent()
{
	return d->_Ascent;
}

double LTypeFace::Descent()
{
	return d->_Descent;
}

double LTypeFace::Leading()
{
	return d->_Leading;
}

