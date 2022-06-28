#include "lgi/common/Lgi.h"
#include "lgi/common/FontSelect.h"

LFontType::LFontType(const char *face, int pointsize)
{
	#if defined WINNATIVE

	ZeroObj(Info);
	if (face)
	{
		swprintf_s(Info.lfFaceName, CountOf(Info.lfFaceName), L"%S", face);
	}
	if (pointsize)
	{
		Info.lfHeight = WinHeightToPoint(pointsize);
	}

	#else

	if (face) Info.Face(face);
	if (pointsize) Info.PointSize(pointsize);

	#endif
}

LFontType::~LFontType()
{
}

const char *LFontType::GetFace()
{
	#ifdef WINNATIVE
	Buf = Info.lfFaceName;
	return Buf;
	#else
	return Info.Face();
	#endif
}

void LFontType::SetFace(const char *Face)
{
	#ifdef WINNATIVE
	if (Face)
	{
		swprintf_s(Info.lfFaceName, CountOf(Info.lfFaceName), L"%S", Face);
	}
	else
	{
		Info.lfFaceName[0] = 0;
	}
	#else
	Info.Face(Face);
	#endif
}

int LFontType::GetPointSize()
{
	#ifdef WINNATIVE
	return WinHeightToPoint(Info.lfHeight);
	#else
	return Info.PointSize();
	#endif
}

void LFontType::SetPointSize(int PointSize)
{
	#ifdef WINNATIVE
	Info.lfHeight = WinPointToHeight(PointSize);
	#else
	Info.PointSize(PointSize);
	#endif
}

bool LFontType::DoUI(LView *Parent)
{
	bool Status = false;

	#if WINNATIVE
	void *i = &Info;
	int bytes = sizeof(Info);
	#else
	char i[128];
	int bytes = sprintf_s(i, sizeof(i), "%s,%i", Info.Face(), Info.PointSize());
	#endif

	LFontSelect Dlg(Parent, i, bytes);
	if (Dlg.DoModal() == IDOK)
	{
		#if WINNATIVE
		Dlg.Serialize(i, sizeof(Info), true);
		#else
		if (Dlg.Face) Info.Face(Dlg.Face);
		Info.PointSize(Dlg.Size);
		#endif

		Status = true;
	}

	return Status;
}

bool LFontType::GetDescription(char *Str, int SLen)
{
	if (Str && GetFace())
	{
		sprintf_s(Str, SLen, "%s, %i pt", GetFace(), GetPointSize());
		return true;
	}

	return false;
}

bool LFontType::Serialize(LDom *Options, const char *OptName, bool Write)
{
	bool Status = false;

	if (Options && OptName)
	{
		LVariant v;
		#if defined WINNATIVE
		if (Write)
		{
			v.SetBinary(sizeof(Info), &Info);
			Status = Options->SetValue(OptName, v);
		}
		else
		{
			if (Options->GetValue(OptName, v))
			{
				if (v.Type == GV_BINARY &&
					v.Value.Binary.Length == sizeof(Info))
				{
					memcpy(&Info, v.Value.Binary.Data, sizeof(Info));
					Status = ValidStrW(Info.lfFaceName);
				}
			}
		}
		#else
		if (Write)
		{
			char Temp[128];
			sprintf_s(Temp, sizeof(Temp), "%s,%i pt", Info.Face(), Info.PointSize());
			Status = Options->SetValue(OptName, v = Temp);
		}
		else
		{
			if (Options->GetValue(OptName, v) && ValidStr(v.Str()))
			{
				char *Comma = strchr(v.Str(), ',');
				if (Comma)
				{
					*Comma++ = 0;
					int PtSize = atoi(Comma);
					if (stricmp(v.Str(), "(null)"))
					{
						Info.Face(v.Str());
						Info.PointSize(PtSize);
						// printf("FontTypeSer getting '%s' = '%s' pt %i\n", OptName, v.Str(), PtSize);
						Status = true;
					}
				}
			}
		}
		#endif
	}

	return Status;
}

bool LFontType::GetConfigFont(const char *Tag)
{
	bool Status = false;

	auto Font = LAppInst->GetConfig(Tag);
	if (Font)
	{
		// read from config file
		auto p = Font.Split(":");
		if (p.Length() == 2)
		{
			SetFace(p[0]);
			SetPointSize((int)p[1].Int());
			Status = true;
		}
		else LgiTrace("%s:%i - Font specification should be: <font>:<pointSize>\n", _FL);
	}

	return Status;
}

class GFontTypeCache
{
	char DefFace[64];
	int DefSize;
	char Face[64];
	int Size;

public:
	GFontTypeCache(char *defface, int defsize)
	{
		ZeroObj(Face);
		Size = -1;
		strcpy_s(DefFace, sizeof(DefFace), defface);
		DefSize = defsize;
	}
	
	char *GetFace(char *Type = 0)
	{
		#ifdef LINUX
		if (!IsInit())
		{
			char f[256];
			int s;
			if (_GetSystemFont(Type, f, sizeof(f), s))
			{
				strcpy_s(Face, sizeof(Face), f);
				Size = s;
			}
			else
			{
				Face[0] = 0;
				Size = 0;
			}
		}
		#endif

		return ValidStr(Face) ? Face : DefFace;
	}
	
	int GetSize()
	{
		return Size > 0 ? Size : DefSize;
	}
	
	bool IsInit()
	{
		return Size >= 0;
	}
};

#if defined USE_CORETEXT

bool MacGetSystemFont(LTypeFace &Info, CTFontUIFontType Which)
{
	CTFontRef ref = CTFontCreateUIFontForLanguage(Which, 0.0, NULL);
	if (!ref)
		return false;

	bool Status = false;
	CFStringRef name = CTFontCopyFamilyName(ref);
	if (name)
	{
		CGFloat sz = CTFontGetSize(ref);
		LString face(name);

		Info.Face(face);
		Info.PointSize((int)sz);

		CFRelease(name);
		Status = true;
	}

	CFRelease(ref);

	return Status;
}

#endif

bool LFontType::GetSystemFont(const char *Which)
{
	bool Status = false;

	if (!Which)
	{
		LAssert(!"No param supplied.");
		return false;
	}
	
	#if LGI_SDL

	#elif defined WINNATIVE

		// Get the system settings
		NONCLIENTMETRICS info;
		info.cbSize = sizeof(info);
		#if (WINVER >= 0x0600)
		LArray<int> Ver;
		if (LGetOs(&Ver) == LGI_OS_WIN32 &&
			Ver[0] <= 5)
			info.cbSize -= 4;
		#endif
		BOOL InfoOk = SystemParametersInfo(	SPI_GETNONCLIENTMETRICS,
											info.cbSize,
											&info,
											0);
		if (!InfoOk)
		{
			LgiTrace("%s:%i - SystemParametersInfo failed with 0x%x (info.cbSize=%i, os=%i, %i)\n",
				_FL, GetLastError(),
				info.cbSize,
				LGetOs(), LGI_OS_WIN9X);
		}

		// Convert windows font height into points
		int Height = WinHeightToPoint(info.lfMessageFont.lfHeight);

	#elif defined __GTK_H__

		// Define some defaults.. in case the system settings aren't there
		static char DefFont[64] =
		#ifdef __CYGWIN__
			"Luxi Sans";
		#else
			"Sans";
		#endif
		int DefSize = 10;
		// int Offset = 0;

		static bool First = true;
		if (First)
		{
			bool ConfigFontUsed = false;
			char p[MAX_PATH_LEN];
			LGetSystemPath(LSP_HOME, p, sizeof(p));
			LMakePath(p, sizeof(p), p, ".lgi.conf");
			if (LFileExists(p))
			{
				LAutoString a(LReadTextFile(p));
				if (a)
				{
					LString s;
					s = a.Get();
					LString::Array Lines = s.Split("\n");
					for (int i=0; i<Lines.Length(); i++)
					{
						if (Lines[i].Find("=")>=0)
						{
							LString::Array p = Lines[i].Split("=", 1);
							if (!_stricmp(p[0].Lower(), "font"))
							{
								LString::Array d = p[1].Split(":");
								if (d.Length() > 1)
								{
									strcpy_s(DefFont, sizeof(DefFont), d[0]);
									int PtSize = d[1].Int();
									if (PtSize > 0)
									{
										DefSize = PtSize;
										ConfigFontUsed = true;
										
										printf("Config font %s : %i\n", DefFont, DefSize);
									}
								}
							}
						}
					}
				}
				else printf("Can't read '%s'\n", p);
			}
			
			if (!ConfigFontUsed)
			{	
				Gtk::GtkStyle *s = Gtk::gtk_style_new();
				if (s)
				{
					const char *fam = Gtk::pango_font_description_get_family(s->font_desc);
					if (fam)
					{
						strcpy_s(DefFont, sizeof(DefFont), fam);
					}
					else printf("%s:%i - pango_font_description_get_family failed.\n", _FL);

					if (Gtk::pango_font_description_get_size_is_absolute(s->font_desc))
					{
						float Px = Gtk::pango_font_description_get_size(s->font_desc) / PANGO_SCALE;
						float Dpi = (float)LScreenDpi().x;
						DefSize = (Px * 72.0) / Dpi;
						printf("pango px=%f, Dpi=%f\n", Px, Dpi);
					}
					else
					{
						DefSize = Gtk::pango_font_description_get_size(s->font_desc) / PANGO_SCALE;
					}
					
					g_object_unref(s);
				}
				else printf("%s:%i - gtk_style_new failed.\n", _FL);
			}
			
			First = false;
		}
	
	#endif

	if (!_stricmp(Which, "System"))
	{
		Status = GetConfigFont("Font-System");
		if (!Status)
		{
			// read from system
			#if LGI_SDL

				#if defined(WIN32)
					Info.Face("Tahoma");
					Info.PointSize(11);
					Status = true;
				#elif defined(MAC)
					Info.Face("LucidaGrande");
					Info.PointSize(11);
					Status = true;
				#elif defined(LINUX)
					Info.Face("Sans");
					Info.PointSize(11);
					Status = true;
				#else
					#error fix me
				#endif
			
			#elif defined WINNATIVE

				if (InfoOk)
				{
					// Copy the font metrics
					memcpy(&Info, &info.lfMessageFont, sizeof(Info));
					Status = true;
				}
				else LgiTrace("%s:%i - Info not ok.\n", _FL);

			#elif defined(HAIKU)

				font_family family = {0};
				font_style style = {0};
				be_plain_font->GetFamilyAndStyle(&family, &style);
				Info.PointSize(be_plain_font->Size());
				Info.Face(family);
				Status = true;

			#elif defined __GTK_H__

				Info.Face(DefFont);
				Info.PointSize(DefSize);
				Status = true;
		
			#elif defined MAC
			

				#ifdef USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontControlContent);

				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeSmallSystemFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size);
					Status = true;
					
					// printf("System=%s,%i\n", Info.Face(), Size);
				}
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Menu"))
	{
		Status = GetConfigFont("Font-Menu");
		if (!Status)
		{
			#if LGI_SDL
			
			LAssert(!"Impl me.");
			
			#elif defined WINNATIVE

			if (InfoOk)
			{
				// Copy the font metrics
				memcpy(&Info, &info.lfMenuFont, sizeof(Info));
				Status = true;
			}

			#elif defined __GTK_H__

			Info.Face(DefFont);
			Info.PointSize(DefSize);
			Status = true;

			#elif defined MAC && !LGI_COCOA
			
				#if USE_CORETEXT

					Status = MacGetSystemFont(Info, kCTFontUIFontMenuItem);

				#else

					Str255 Name;
					SInt16 Size;
					Style St;
					OSStatus e = GetThemeFont(	kThemeMenuItemFont,
												smSystemScript,
												Name,
												&Size,
												&St);
					if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
					else
					{
						Info.Face(p2c(Name));
						Info.PointSize(Size);
						Status = true;
					}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Caption"))
	{
		Status = GetConfigFont("Font-Caption");
		if (!Status)
		{
			#if LGI_SDL
			
				LAssert(!"Impl me.");
			
			#elif defined WINNATIVE

				if (InfoOk)
				{
					// Copy the font metrics
					memcpy(&Info, &info.lfCaptionFont, sizeof(Info));
					Status = true;
				}
			
			#elif defined LINUX

			#elif defined __GTK_H__

				Info.Face(DefFont);
				Info.PointSize(DefSize-1);
				Status = true;

			#elif defined MAC && !LGI_COCOA
			
				#if USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontToolbar);
			
				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeToolbarFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size);
					Status = true;
				}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Status"))
	{
		Status = GetConfigFont("Font-Status");
		if (!Status)
		{
			#if LGI_SDL
			
				LAssert(!"Impl me.");
			
			#elif defined WINNATIVE
			
			if (InfoOk)
			{
				// Copy the font metrics
				memcpy(&Info, &info.lfStatusFont, sizeof(Info));
				Status = true;
			}
			
			#elif defined __GTK_H__

			Info.Face(DefFont);
			Info.PointSize(DefSize);
			Status = true;

			#elif defined MAC && !LGI_COCOA
			
				#if USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontSystemDetail);
			
				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeToolbarFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size);
					Status = true;
				}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Small"))
	{
		Status = GetConfigFont("Font-Small");
		if (!Status)
		{
			#if LGI_SDL
			
				LAssert(!"Impl me.");
			
			#elif defined WINNATIVE

			if (InfoOk)
			{
				// Copy the font metrics
				memcpy(&Info, &info.lfSmCaptionFont, sizeof(Info));
				if (LGetOs() == LGI_OS_WIN9X && _wcsicmp(Info.lfFaceName, L"ms sans serif") == 0)
				{
					SetFace("Arial");
				}

				// Make it a bit smaller than the system font
				Info.lfHeight = WinPointToHeight(WinHeightToPoint(info.lfMessageFont.lfHeight)-1);
				Info.lfWeight = FW_NORMAL;

				Status = true;
			}

			#elif defined __GTK_H__

			Info.Face(DefFont);
			Info.PointSize(DefSize-1);
			Status = true;
			
			#elif defined MAC
			
				#if USE_CORETEXT

				Status = MacGetSystemFont(Info, kCTFontUIFontSmallSystem);
			
				#else
			
				Str255 Name;
				SInt16 Size;
				Style St;
				OSStatus e = GetThemeFont(	kThemeSmallSystemFont,
											smSystemScript,
											Name,
											&Size,
											&St);
				if (e) printf("%s:%i - GetThemeFont failed with %i\n", __FILE__, __LINE__, (int)e);
				else
				{
					Info.Face(p2c(Name));
					Info.PointSize(Size - 2);
					Status = true;
				}
			
				#endif

			#endif
		}
	}
	else if (!stricmp(Which, "Fixed"))
	{
		Status = GetConfigFont("Font-Fixed");
		if (!Status)
		{
			#if LGI_SDL
			
				LAssert(!"Impl me.");
			
			#elif defined WINNATIVE

				// SetFace("Courier New");
				SetFace("Consolas");
				Info.lfHeight = WinPointToHeight(10);
				Status = true;

			#elif defined(HAIKU)

				font_family family = {0};
				font_style style = {0};
				be_fixed_font->GetFamilyAndStyle(&family, &style);
				Info.PointSize(be_fixed_font->Size());
				Info.Face(family);
				Status = true;

			#elif defined __GTK_H__

				Info.Face("Courier New");
				Info.PointSize(DefSize);
				Status = true;
			
			#elif defined MAC
			
				Status = MacGetSystemFont(Info, kCTFontUIFontUserFixedPitch);

			#else

			#warning "Impl me"

			#endif
		}
	}
	else
	{
		LAssert(!"Invalid param supplied.");
	}
	
	// printf("GetSystemFont(%s)=%i %s,%i\n", Which, Status, Info.Face(), Info.PointSize());
	return Status;
}

bool LFontType::GetFromRef(OsFont Handle)
{
	#if defined WIN32
	return GetObject(Handle, sizeof(Info), &Info) == sizeof(Info);
	#else
	// FIXME
	return false;
	#endif
}

LFont *LFontType::Create(LSurface *pSurface)
{
	LFont *New = new LFont;
	if (New)
	{
		if (!New->Create(this, pSurface))
		{
			DeleteObj(New);
		}
		
		if (New)
			LAssert(New->GetHeight() > 0);
	}
	return New;
}

