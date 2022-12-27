/*hdr
**      FILE:           LDb.h
**      AUTHOR:         Matthew Allen
**      DATE:           8/2/2000
**      DESCRIPTION:    Db API definitions
**
**      Copyright (C) 2000 Matthew Allen
**						fret@memecode.com
*/

#ifndef __GDB_H
#define __GDB_H

#include "lgi/common/Variant.h"

#define Dom_IsConnected		"IsConnected"

// Classes
class LDbField
{
public:
	virtual ~LDbField() {};

	// Cast
	virtual operator char*() { LAssert(0); return 0; }
	virtual LDbField &operator =(char*) { LAssert(0); return *this; }

	// Properties
	virtual const char *Name() { return 0; }
	virtual bool Name(const char *Str) { return 0; }
	virtual LVariantType Type() { return GV_NULL; }
	virtual bool Type(LVariantType NewType) { return 0; }
	virtual int Length() { return 0; }
	virtual bool Length(int NewLength) { return 0; }
	virtual const char *Description() { return 0; }
	virtual bool Description(const char *NewDesc) { return 0; }

	// Set/Get
	virtual bool Set(LVariant &v) { return 0; }
	virtual bool Get(LVariant &v) { return 0; }
};

class LDbRecordset
{
public:
	virtual ~LDbRecordset() {};

	// Props
	virtual char *Name() = 0;

	// Fields
	virtual LDbField &operator [](unsigned Index) = 0;
	virtual LDbField &	operator [](const char *Name) = 0;
	virtual LDbField *InsertField(	const char *Name,
									LVariantType Type = GV_STRING,
									int Length = 0,
									int Index = -1) = 0; // default: append to the end
	virtual bool DeleteField(LDbField *Fld) = 0;
	virtual int Fields() = 0; // return number of fields

	// Markers
	virtual bool End() = 0;

	// New/Edit
	virtual bool AddNew() = 0;
	virtual bool Edit() = 0;
	virtual bool Update() = 0;
	virtual void Cancel() = 0;

	// Moving around the records
	virtual bool MoveFirst() = 0;
	virtual bool MoveNext() = 0;
	virtual bool MovePrev() = 0;
	virtual bool MoveLast() = 0;
	virtual int SeekRecord(int i) = 0; // from the beginning

	// Numbers/Counts
	virtual int RecordIndex() = 0;
	virtual int Records() = 0;
	
	// Deleting
	virtual bool DeleteRecord() = 0; // delete's the current record
};

class LDb : public LDom
{
public:
	virtual ~LDb() {}

	// Attaching to data
	virtual bool Connect(const char *Init) = 0;
	virtual bool Disconnect() = 0;

	// Retrieving data
	virtual LDbRecordset *Open(char *Name) = 0;
	virtual bool Execute(char *Name) { return false; }
	virtual LDbRecordset *TableAt(int i) { return NULL; }
	virtual const char *GetErrorMsg() { return 0; }
};

// Actual implementations
extern LDb *OpenDbfDatabase(const char *s);			// add 'Db-Dbf.cpp' to your project
extern LDb *OpenAdoDatabase(const char *s);			// add 'Db-Ado.cpp' to your project
extern LDb *OpenCsvDatabase(const char *s, bool HasHeader = true);		// add 'Db-Csv.cpp' to your project
extern LDb *OpenTsvDatabase(const char *s, bool HasHeader = true);		// add 'Db-Csv.cpp' to your project
extern LDb *OpenMetakitDatabase(const char *s);		// add 'Db-Metakit.cpp' to your project
extern LDb *OpenBfsDatabase(const char *s);			// add 'Db-Bfs.cpp' and 'Bfs.cpp' to your project
extern LDb *OpenLibmysqlDatabase(const char *s);	// add 'LDb-Libmysql.cpp'
													// pass 'mysql://username:password@host/database'
													// (will cause your app to become GPL)
extern LDb *OpenMysqlDirectDatabase(char *s);		// add 'Db-Mysql-direct.cpp'
													// pass 'mysql://username:password@host/database'
													// (will _NOT_ cause your app to become GPL)

// Util
extern char *LgiTsvTok(char *&s);


#endif
