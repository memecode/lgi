_(ObjNone, NULL)
	
// Common
_(ObjLength, "Length")
_(ObjType, "Type")
_(ObjName, "Name")
_(ObjIdentifier, "Id")
_(ObjStyle, "Style")
_(ObjClass, "Class")
_(ObjField, "Field")
_(ObjDebug, "Debug")
_(ObjTextContent, "textContent")
_(ObjInnerHtml, "innerHTML")

// Types
_(TypeList, "List")
_(TypeHashTable, "HashTable")
_(TypeFile, "File")
_(TypeSurface, "Surface")
_(TypeDateTime, "DateTime")
_(TypeInt, "Int")
_(TypeDouble, "Double")
_(TypeString, "String")

// LDateTime
_(DateYear, "Year")
_(DateMonth, "Month")
_(DateDay, "Day")
_(DateHour, "Hour")
_(DateMinute, "Minute")
_(DateSecond, "Second")
_(DateDate, "Date") // "yyyymmdd"
_(DateTime, "Time") // "hhmmss"
_(DateDateAndTime, "DateAndTime") // "yyyymmdd hhmmss"
_(DateTimestamp, "Timestamp") // 64bit UTC
_(DateSetNow, "SetNow")
_(DateSetStr, "SetStr")
_(DateGetStr, "GetStr")
_(DateSecond64Bit, "Second64Bit")

// LVariant string
_(StrJoin, "Join")
_(StrSplit, "Split")
_(StrSplitDelimit, "SplitDelimit")
_(StrFind, "Find")
_(StrRfind, "Rfind")
_(StrLower, "Lower")
_(StrUpper, "Upper")
_(StrStrip, "Strip")
_(StrSub, "Sub")
	
// LSurface
_(SurfaceX, "X")
_(SurfaceY, "Y")
_(SurfaceBits, "Bits")
_(SurfaceColourSpace, "ColourSpace")
_(SurfaceIncludeCursor, "IncludeCursor")
	
// List GHashTbl/LHashTbl
_(ContainerAdd, "Add")
_(ContainerDelete, "Delete")
_(ContainerHasKey, "HasKey")
_(ContainerHasItem, "HasItem")
_(ContainerSort, "Sort")
_(ContainerChildren, "Children")
_(ContainerSpan, "Span")
_(ContainerAlign, "Align")
_(ContainerVAlign, "VAlign")
	
// LFile
_(FileOpen, "Open")
_(FileRead, "Read")
_(FileWrite, "Write")
_(FilePos, "Pos")
_(FileClose, "Close")
_(FileModified, "Modified")
_(FileFolder, "Folder")
_(FileEncoding, "Encoding")
	
// LStream
_(StreamReadable, "Readable")
_(StreamWritable, "Writable")

// Rich text editor
_(HtmlImagesLinkCid, "HtmlImagesLinkCid") // output CID image links rather than files.
_(SpellCheckLanguage, "SpellCheckLanguage")
_(SpellCheckDictionary, "SpellCheckDictionary")

// Menu
_(AppendSeparator, "AppendSeparator")
_(AppendItem, "AppendItem")
_(AppendSubmenu, "AppendSubmenu")

// Ctrl Table
_(TableLayoutCols, "Cols")
_(TableLayoutRows, "Rows")
_(TableLayoutCell, "Cell")

// ControlTree
_(ControlSerialize, "Serialize")

// LApplicator
_(AppAlpha, "Alpha")			// (uint8_t)
_(AppPalette, "Palette")		// (LPalette*)(void*)
_(AppBackground, "Background")	// (uint32_t) rgba32
_(AppAngle, "Angle")			// (int32_t) degrees
_(AppBounds, "Bounds")			// (LRect*)(void*)
