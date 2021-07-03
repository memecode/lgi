#ifndef _LGI_RES_H_
#define _LGI_RES_H_

/**
 * \defgroup Resources Lgi multi-language resource support
 *
 * The LGI resource module allows you to edit resources in LgiRes 
 * and then load them at runtime. A system of tags allows you to
 * include or exclude controls depending on various conditions. You
 * might have slightly different builds of the software that have
 * different UI.
 *
 * The system also allows full translation of the UI elements into
 * any language. Currently the system only loads one language at runtime
 * to save on memory. To change language you have to restart the 
 * software using Lgi resources.
 *
 * There are two numbers that you can use to access resources:
 * - Reference (Ref): This is a globally unique number for any given 
 *		resource. Multiple controls with the same #define name will
 *      have different Ref numbers.
 * - Identifier (Id): This number is the same for any control having
 *      the same #define name. Usually dialogs only have distinct 
 *      control names internal to themselves. Different dialogs can
 *      reuse the same names (Like ID_NEXT could be uses in several
 *      dialogs). Id's are written to the 'resdefs.h' file that you
 *      include in the source.
 *
 * \ingroup Lgi
 */
#include "lgi/common/Res.h"
#include "lgi/common/Containers.h"
#include "lgi/common/Css.h"
#include "lgi/common/AutoPtr.h"
#include "lgi/common/FontCache.h"

class LResources;

/// A string resource
/// \ingroup Resources
class LgiClass LgiStringRes
{
	LResources *Res;

public:
	static const char *CodePage;
	static GLanguage *CurLang;

	int Ref;
	int Id;
	char *Str;
	char *Tag;
	// bool IsString;

	LgiStringRes(LResources *res);
	~LgiStringRes();

	LResources *GetRes() { return Res; }
	bool Read(LXmlTag *Tag, ResFileFormat Format);
};

/// A dialog resource
/// \ingroup Resources
class LgiClass LgiDialogRes
{
	LResources *Res;

public:
	LXmlTag *Dialog;
	LgiStringRes *Str;
	LRect Pos;

	LgiDialogRes(LResources *res);
	~LgiDialogRes();

	LResources *GetRes() { return Res; }
	bool Read(LXmlTag *Tag, ResFileFormat Format);
	char *Name() { return (Str) ? Str->Str : 0; }
	int Id() { return (Str) ? Str->Id : 0; }
	int X() { return Pos.X(); }
	int Y() { return Pos.Y(); }
};

/// A menu resource
/// \ingroup Resources
class LgiClass LgiMenuRes : public GBase
{
	LResources *Res;
	LHashTbl<IntKey<int>, LgiStringRes*> Strings;

public:
	LXmlTag *Tag;

	LgiMenuRes(LResources *res);
	~LgiMenuRes();

	bool Read(LXmlTag *Tag, ResFileFormat Format);
	LResources *GetRes() { return Res; }
	LgiStringRes *GetString(LXmlTag *Tag);
};

/// A resource collection.
/// \ingroup Resources
class LgiClass LResources : public ResFactory
{
	friend class GLgiRes;
	friend class LMenu;
	friend class LgiStringRes;

	class LgiResourcesPrivate *d;
	List<LgiDialogRes> Dialogs;
	List<LgiMenuRes> Menus;
	GEventsI *ScriptEngine;

	/// Array of languages available in the loaded file.
	GArray<GLanguageId> Languages; 
	
	/// Add a language to the Languages array
	void AddLang(GLanguageId id);

	/// If this is true then all UI elements should attempt to load styles from the CSS store
	/// by calling 'StyleElement'. It will default to false for old applications. Newer apps
	/// can enable it manually.
	static bool LoadStyles;

public:
	static bool DefaultColours;
	LHashTbl<ConstStrKey<char>, char*> LanguageNames;
	
	/// This is all the CSS loaded from the lr8 file (and possibly other sources as well)
	GCss::Store CssStore;

	/// Get the load styles setting (enable 'StyleElement' to do something)
	static bool GetLoadStyles() { return LoadStyles; }
	
	/// Sets the loading of styles for all UI elements.
	static void SetLoadStyles(bool ls) { LoadStyles = ls; }

	/// This is called by UI elements to load styles if necessary.
	static bool StyleElement(GViewI *v);
	
	const char *GetThemeFolder();
	void SetThemeFolder(const char *f);

	/// The constructor
	LResources
	(
		/// [optional] The filename to use.
		const char *FileName = NULL,
		/// [optional] Warn if the file is not found.
		bool Warn = false,
		/// [optional] Folder for theming info
		const char *ThemeFolder = NULL
	);
	virtual ~LResources();

	/// Loads a dialog from the resource into the UI.
	/// \return true on success.
	bool LoadDialog
	(
		/// The ID of the resource
		int Resource,
		/// The view to contain all the new controls.
		GViewI *Parent,
		/// [Optional] The size of the dialog if needed
		LRect *Pos = NULL,
		/// [Optional] The name of the window.
		GAutoString *Name = NULL,
		/// [Optional] A scripting engine interface
		GEventsI *ScriptEngine = NULL,
		/// [Optional] The current tags list for optional 
		/// inclusion / exclusion of controls.
		char *Tags = 0
	);
	
	/// Get a string resource object using it's reference.
	LgiStringRes *StrFromRef(int Ref);
	
	/// Gets the value of a string resource from it's Ref.
	char *StringFromRef(int Ref);

	/// Gets the value of a string resource from it's Id.
	char *StringFromId(int Ref);

	/// \returns true if the object loaded ok.	
	bool IsOk();
	
	/// Gets the file format.
	ResFileFormat GetFormat();

	/// Load a specific file
	bool Load(const char *FileName);
	
	/// Gets the filename used to load the object.
	char *GetFileName();
	
	/// \returns the languages in the file.
	GArray<GLanguageId> *GetLanguages() { return &Languages; }
	
	/// \returns an iterator for all the dialogs in the resource collection.
	List<LgiDialogRes>::I GetDialogs() { return Dialogs.begin(); }

	/// Create a resource object
	/// \private
	ResObject *CreateObject(LXmlTag *Tag, ResObject *Parent);

	/// Sets the position of an object
	/// \private
	void Res_SetPos(ResObject *Obj, int x1, int y1, int x2, int y2);

	/// Create a resource object from a string
	/// \private
	void Res_SetPos(ResObject *Obj, char *s);

	/// Gets the position
	/// \private
	LRect Res_GetPos(ResObject *Obj);

	/// Gets the string ref number
	/// \private
	int Res_GetStrRef(ResObject *Obj);

	/// Sets the string ref associated with a control
	/// \private
	bool Res_SetStrRef(ResObject *Obj, int Ref, ResReadCtx *Ctx);

	/// Attach an object to another (create a parent / child relationship)
	/// \private
	void Res_Attach(ResObject *Obj, ResObject *Parent);

	/// Gets all the child objects
	/// \private
	bool Res_GetChildren(ResObject *Obj, List<ResObject> *l, bool Deep);

	/// Appends an object to a parent
	/// \private
	void Res_Append(ResObject *Obj, ResObject *Parent);

	/// ?
	/// \private
	bool Res_GetItems(ResObject *Obj, List<ResObject> *l);

	/// Gets a dom object of properties
	/// \private
	bool Res_GetProperties(ResObject *Obj, GDom *Props);

	/// Sets a dom object of properties
	/// \private
	bool Res_SetProperties(ResObject *Obj, GDom *Props);

	/// Gets the current dom object of properties
	/// \private
	GDom* Res_GetDom(ResObject *Obj);
};

/// Loads a resource and returns a pointer to it.
/// \ingroup Resources
LgiExtern LResources *LgiGetResObj(bool Warn = false, const char *filename = 0, bool LoadOnDemand = true);

/// This class is used to style GView controls with CSS
class LgiClass GViewCssCb : public GCss::ElementCallback<GViewI>
{
public:
	const char *GetElement(GViewI *obj)
	{
		return obj->GetClass();
	}
	
	const char *GetAttr(GViewI *obj, const char *Attr)
	{
		return NULL;
	}
	
	bool GetClasses(GString::Array &Classes, GViewI *obj)
	{
		GString::Array *a = obj->CssClasses();
		if (!a)
			return false;
		Classes = *a;
		return true;
	}
	
	GViewI *GetParent(GViewI *obj)
	{
		return obj->GetParent();
	}
	
	GArray<GViewI*> GetChildren(GViewI *obj)
	{
		return obj->IterateViews();
	}
};

/// Loads a string from the resource file
/// \ingroup Resources
LgiFunc const char *LgiLoadString(int Res, const char *Default = 0);


#endif