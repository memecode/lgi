#ifndef _GZIPFILE_H_
#define _GZIPFILE_H_

class LZipFile
{
	class LZipFilePrivate *d;

public:
	LZipFile(char *zip = 0);
	~LZipFile();
	
	bool IsOpen();
	bool Open(char *Zip, int Mode);
	void Close();
	
	LDirectory *List();
	bool Decompress(char *File, LStream *To);
	bool Compress(char *File, LStream *From);
	bool Delete(char *File);
};

#endif