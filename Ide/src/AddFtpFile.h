#ifndef _ADD_FTP_FILE_H_
#define _ADD_FTP_FILE_H_

#include "FtpThread.h"

class AddFtpFile : public LDialog, public FtpCallback
{
	LUri *Base;
	LList *Files;
	LList *Log;
	FtpThread *Thread;

public:
	LArray<char*> Uris;

	AddFtpFile(LViewI *p, char *ftp);
	~AddFtpFile();

	void OnCmdComplete(FtpCmd *Cmd);
	int OnNotify(LViewI *c, LNotification n);
};

#endif