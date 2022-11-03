/*hdr
**      FILE:           LDb-Dbf.h
**      AUTHOR:         Matthew Allen
**      DATE:           8/2/2000
**      DESCRIPTION:    Dbf implementation of the LDb API
**
**      Copyright (C) 2000 Matthew Allen
**						fret@memecode.com
*/

#include <stdio.h>
#include <stdlib.h>

#include "Lgi.h"
#include "LDb.h"

///////////////////////////////////////////////////////////////////
// Forward decl
class LDbfField;
class LDbfRecord;
class LDbfRecordset;

// Classes
class LDbfDb : public LDb
{
	char *Dir;

public:
	LDbfDb();
	~LDbfDb();

	// Attaching to data
	bool Connect(char *Init);
	bool Disconnect() { return true; }

	// Retrieving data
	LDbRecordset *Open(char *Name);
	LDbRecordset *TableAt(int i) { return 0; }
};

class LDbfRecord
{
	LDbfRecordset *Recordset;
	char *Data;

public:
	LDbfRecord(LDbfRecordset *rs);
	virtual ~LDbfRecord();

	char *GetData();
};

class LDbfField : public LDbField
{
	friend class LDbfRecord;
	friend class LDbfRecordset;

	LDbfRecordset *Recordset;

	char *FldName;
	int FldType;
	int FldLength;
	int MemLength;
	int DiskOffset; // into record data
	int MemOffset; // into record data
	short Decimal; // position of decimal point

	char *GetPtr();

public:
	LDbfField(LDbfRecordset *rs);
	~LDbfField();

	// Properties
	char *Name();
	bool Name(char *Str);
	int Type();
	bool Type(int NewType);
	int Length();
	bool Length(int NewLength);

	// Data access
	bool Set(LVariant &v);
	bool Get(LVariant &v);

	// Not implemented
	char *Description() { return 0; }
	bool Description(char *NewDesc) { return false; }
	bool Set(void *Data, int Length) { return false; }
	bool Get(void *&Data, int &Length) { return false; }
};

class LDbfRecordset : public LDbRecordset
{
	friend class LDbfRecord;
	friend class LDbfField;

	char *FileName;				// Name of DBF file
	LFile *File;					// Open file
	
	int MemLength; // memory record length
	ushort DiskLength; // disk record length
	ushort HeaderLength;
	uint RecordCount;
	
	List<LDbfField> F;
	List<LDbfRecord> R;
	LDbfRecord *Current;
	LDbfField *NullField;

public:
	LDbfRecordset(char *file);
	~LDbfRecordset();

	// State
	bool IsOpen()
	{
		return (File!=0) AND (FileName!=0) AND (F.GetItems()>0);
	}

	// Fields
	LDbField *operator [](int Index);
	LDbField *operator [](char *Name);
	bool InsertField(LDbField *Fld, int Index = -1);
	bool DeleteField(LDbField *Fld);
	int Fields();

	bool AddNew() { return false; }
	bool Edit() { return false; }
	bool Update() { return false; }
	void Cancel() { }

	// Markers
	bool Bof();
	bool Eof();

	// Moving around the records
	bool MoveFirst();
	bool MoveNext();
	bool MovePrev();
	bool MoveLast();
	int SeekRecord(int i);

	// Numbers/Counts
	int RecordIndex();
	int CountRecords();
	
	// Deleting
	bool DeleteRecord();
};

///////////////////////////////////////////////////////////////////
LDbfDb::LDbfDb()
{
	Dir = 0;
}

LDbfDb::~LDbfDb()
{
	DeleteArray(Dir);
}

bool LDbfDb::Connect(char *Init)
{
	DeleteArray(Dir);
	Dir = NewStr(Init);
	return Dir != 0;
}

LDbRecordset *LDbfDb::Open(char *Name)
{
	if (Dir AND Name)
	{
		char d[256];
		char *LastChar = Dir + (strlen(Dir)-1);
		if (*LastChar == DIR_CHAR) *LastChar = 0;

		sprintf(d, "%s%c%s", Dir, DIR_CHAR, Name);
		if (FileExists(d))
		{
			return new LDbfRecordset(d);
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////
LDbfRecord::LDbfRecord(LDbfRecordset *rs)
{
	Recordset = rs;
	Data = 0;
}

LDbfRecord::~LDbfRecord()
{
	DeleteArray(Data);
}

char *LDbfRecord::GetData()
{
	if (!Data AND
		Recordset AND
		Recordset->IsOpen())
	{
		// Get my index
		int Index = Recordset->R.IndexOf(this);
		if (Index >= 0)
		{
			// Allocate space
			Data = new char[Recordset->MemLength];
			if (Data)
			{
				// Seek to start of the record
				Recordset->File->Seek(	Recordset->HeaderLength +
										(Recordset->DiskLength * Index) + 1,
										SEEK_SET);
				
				// Read the record in
				if (Recordset->File->Read(Data, Recordset->DiskLength-1) == (Recordset->DiskLength-1))
				{
					for (LDbfField *f = Recordset->F.Last();
						f; f = Recordset->F.Prev())
					{
						// move the data into place
						memmove(Data+f->MemOffset, Data+f->DiskOffset, f->FldLength);
						if (f->MemLength > f->FldLength)
						{
							// zero out any space after the field
							// (for null-terminated strings)
							memset(Data+f->MemOffset+f->FldLength, 0, f->MemLength-f->FldLength);
						}

						// truncate trailing spaces in string data
						if (f->FldType == GV_STRING)
						{
							char *Start = Data + f->MemOffset;
							for (char *End = Start + f->FldLength - 1; End >= Start; End--)
							{
								if (*End == ' ')
								{
									*End = 0;
								}
								else break;
							}
						}
					}
				}
				else
				{
					// file is trucated
					DeleteArray(Data);
				}
			}
		}
	}

	return Data;
}

/////////////////////////////////////////////////////////////////////////
LDbfField::LDbfField(LDbfRecordset *rs)
{
	Recordset = rs;
	FldName = 0;
	FldType = 0;
	FldLength = 0;
	MemLength = 0;
	DiskOffset = 0;
	MemOffset = 0;
}

LDbfField::~LDbfField()
{
	DeleteArray(FldName);
}

char *LDbfField::Name()
{
	return FldName;
}

bool LDbfField::Name(char *Str)
{
	if (Str)
	{
		DeleteArray(FldName);
		FldName = NewStr(Str);
		return FldName != 0;
	}

	return false;
}

int LDbfField::Type()
{
	return FldType;
}

bool LDbfField::Type(int NewType)
{
	// TODO: implement field conversion
	return NewType == FldType;
}

int LDbfField::Length()
{
	switch (FldType)
	{
		case GV_INT32:
			return sizeof(int);
		case GV_DOUBLE:
			return sizeof(double);
		case GV_STRING:
			return FldLength;
	}

	return 0;
}

bool LDbfField::Length(int NewLength)
{
	// TODO: implement length conversion
	return false;
}

char *LDbfField::GetPtr()
{
	if (Recordset AND Recordset->Current)
	{
		char *d = Recordset->Current->GetData();
		if (d)
		{
			return d + MemOffset;
		}
	}

	return 0;
}

bool LDbfField::Set(LVariant &v)
{
	char *Data = GetPtr();

	switch (v.Type)
	{
		case GV_STRING:
		{
			char *Str = v.Str();
			int Len = strlen(Str);
			if (Len <= FldLength)
			{
				strcpy(Data, Str);
			}
			else
			{
				memcpy(Data, Str, FldLength);
				// should already have a NULL at the end of the field
			}
			return true;
		}
		case GV_INT32:
		{
			char *Str = GetPtr();
			if (Str)
			{
				char Buf[32];
				sprintf(Buf, "%i", v.Value.Int);
				int Len = strlen(Buf);
				int White = Len - FldLength;
				memset(Str, ' ', White);
				memcpy(Str+White, Buf, Len);
			}
			return false;
		}
		case GV_DOUBLE:
		{
			char *Str = GetPtr();
			if (Str)
			{
				char Format[8];
				char Buf[32];
				sprintf(Format, "%%.%if", Decimal);
				sprintf(Buf, Format, v.Value.Dbl);
				int Len = strlen(Buf);
				int White = Len - FldLength;
				memset(Str, ' ', White);
				memcpy(Str+White, Buf, Len);
			}
		}
		return false;
	}

	return false;
}

bool LDbfField::Get(LVariant &v)
{
	char *Data = GetPtr();

	switch (FldType)
	{
		case GV_STRING:
		{
			v = GetPtr();
			return true;
		}
		case GV_INT32:
		{
			v = atoi(Data);
			return true;
		}
		case GV_DOUBLE:
		{
			v = atof(Data);
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////
LDbfRecordset::LDbfRecordset(char *file)
{
	FileName = NewStr(file);
	File = (FileName) ? new LFile : 0;
	DiskLength = 0;
	MemLength = 0;
	HeaderLength = 0;
	RecordCount = 0;
	Current = 0;
	NullField = new LDbfField(0);

	int MemOffset = 0;
	if (File AND File->Open(FileName, O_READ))
	{
		// Read header
		uchar Type;
		char LastUpdate[3];
		
		*File >> Type;
		File->Read(LastUpdate, 3);
		*File >> RecordCount;
		*File >> HeaderLength;
		*File >> DiskLength;
		File->Seek(32, SEEK_SET);

		// Read fields
		while (1)
		{
			char Name[16];
			ZeroObj(Name);
			File->Read(Name, 11);
			if (Name[0] != 0x0d)
			{
				LDbfField *Fld = new LDbfField(this);
				if (Fld)
				{
					Fld->FldName = NewStr(Name);
					
					uchar Type;
					*File >> Type;
					File->Seek(4, SEEK_CUR);
					uchar Length;
					*File >> Length;
					Fld->FldLength = Length;
					uchar Dec;
					*File >> Dec;
					Fld->Decimal = Dec;
					File->Seek(14, SEEK_CUR);

					switch (Type)
					{
						case 'C':
						case 'c':
						{
							Fld->FldType = GV_STRING;
							break;
						}
						case 'D':
						case 'd':
						{
							Fld->FldType = GV_DATETIME;
							break;
						}
						case 'F':
						case 'f':
						{
							Fld->FldType = GV_DOUBLE;
							break;
						}
						case 'L':
						case 'l':
						{
							Fld->FldType = GV_BINARY;
							break;
						}
						case 'M':
						case 'm':
						{
							Fld->FldType = GV_BINARY;
							break;
						}
						case 'N':
						case 'n':
						{
							Fld->FldType = (Dec>0) ? GV_DOUBLE : GV_INT32;
							break;
						}
						default:
						{
							// unknown field type
							break;
						}
					}

					Fld->DiskOffset = MemOffset;
					MemOffset += Fld->FldLength;
					Fld->MemOffset = MemLength;
					// add a NULL char for strings
					Fld->MemLength = Fld->FldLength + 1;
					MemLength += Fld->MemLength;

					// add field
					F.Insert(Fld);
				}
				else
				{
					// memory allocation error
					LAssert(0);
					break;
				}
			}
			else
			{
				// end of field list...
				break;
			}
		} // loop to next field

		// Create record objects
		for (uint i=0; i<RecordCount; i++)
		{
			R.Insert(new LDbfRecord(this));
		}
	}
}

LDbfRecordset::~LDbfRecordset()
{
	F.DeleteObjects();
	R.DeleteObjects();
	Current = 0;
}

LDbField *LDbfRecordset::operator [](int Index)
{
	LDbfField *f = F.ItemAt(Index);
	return (f) ? f : NullField;
}

LDbField *LDbfRecordset::operator [](char *Name)
{
	if (Name)
	{
		for (LDbfField *f = F.First(); f; f = F.Next())
		{
			if (f->Name() AND
				stricmp(f->Name(), Name) == 0)
			{
				return f;
			}
		}
	}

	return NullField;
}

bool LDbfRecordset::InsertField(LDbField *Fld, int Index)
{
	// TODO
	return false;
}

bool LDbfRecordset::DeleteField(LDbField *Fld)
{
	// TODO
	return false;
}

int LDbfRecordset::Fields()
{
	return F.GetItems();
}

bool LDbfRecordset::Bof()
{
	return Current == 0;
}

bool LDbfRecordset::Eof()
{
	return Current == 0;
}

bool LDbfRecordset::MoveFirst()
{
	Current = R.First();
	return Current != 0;
}

bool LDbfRecordset::MoveNext()
{
	Current = R.Next();
	return Current != 0;
}

bool LDbfRecordset::MovePrev()
{
	Current = R.Prev();
	return Current != 0;
}

bool LDbfRecordset::MoveLast()
{
	Current = R.Last();
	return Current != 0;
}

int LDbfRecordset::SeekRecord(int i)
{
	Current = R.ItemAt(i);
	return Current != 0;
}

int LDbfRecordset::RecordIndex()
{
	return (Current) ? R.IndexOf(Current) : -1;
}

int LDbfRecordset::CountRecords()
{
	return R.GetItems();
}

bool LDbfRecordset::DeleteRecord()
{
	// TODO
	return false;
}

///////////////////////////////////////////////////////////////////
LDb *OpenDbfDatabase(char *Dir)
{
	LDbfDb *Dbf = new LDbfDb;
	if (Dbf AND Dbf->Connect(Dir))
	{
		return Dbf;
	}

	DeleteObj(Dbf);
	return 0;
}

