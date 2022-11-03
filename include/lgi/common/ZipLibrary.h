#ifndef __GZIP_LIB_H
#define __GZIP_LIB_H

class LLogTarget
{
public:
	virtual void ClearLog() = 0;
	virtual void Log(char *Str, int c = 0) = 0;
};

class LZipLibrary
{
	class LZipLibPrivate *d;

public:
	LZipLibrary(LLogTarget *log);

	bool IsOk();
	bool Zip(char *ZipFile, char *BaseDir, List<char> &Extensions, char *AfterDate);
};

#endif
