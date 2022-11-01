
#ifndef _GWORDSTORE_H_
#define _GWORDSTORE_H_

class LWordStore
{
	struct LWordStorePriv *d;

public:
	LWordStore(const char *file = 0);
	~LWordStore();
	
	// Serialization
	bool Serialize(const char *file, bool Load);
	
	// Container
	long GetItems();
	bool SetItems(int s);
	bool Insert(const char *Word);
	long GetWordCount(const char *Word);
	int SetWordCount(const char *Word, ssize_t Count);
	bool DeleteWord(const char *Word);
	void Empty();
	char *GetFile();
	void SetFile(const char *file);
	unsigned long Length();

	#ifdef _DEBUG
	int64 Sizeof();
	#endif
};

#endif
