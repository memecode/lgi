#ifndef _GZIPFILE_H_
#define _GZIPFILE_H_

class GZipFile
{
	class GZipFilePrivate *d;

public:
	GZipFile(char *zip = 0);
	~GZipFile();
	
	bool IsOpen();
	bool Open(char *Zip, int Mode);
	void Close();
	
	GDirectory *List();
	bool Decompress(char *File, LStream *To);
	bool Compress(char *File, LStream *From);
	bool Delete(char *File);
};

#endif