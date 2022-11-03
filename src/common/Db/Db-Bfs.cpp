#include "Lgi.h"
#include "LDb.h"
#include "Bfs/LgiBfs.h"

class LBfsDb : public LDb
{
	Bfs *Db;

public:
	LBfsDb(char *s)
	{
		Db = BfsCreate(s);
		if (Db)
		{
			int File = BfsOpenFile(Db, "/test.txt", S_IREAD);
			if (File)
			{
				int64 Size = BfsGetFileSize(Db, File);

				char Msg[] = "This is the test data string in the file.\n";
				BfsWriteFile(Db, File, Msg, strlen(Msg));

				Size = BfsGetFileSize(Db, File);
			}
		}
	}

	~LBfsDb()
	{
		DeleteObj(Db);
	}

	bool Connect(char *Init)
	{
		return false;
	}

	bool Disconnect()
	{
		return false;
	}

	LDbRecordset *Open(char *Name)
	{
		return false;
	}

	LDbRecordset *TableAt(int i)
	{
		return false;
	}
};

LDb *OpenBfsDatabase(char *s)
{
	return new LBfsDb(s);
}

