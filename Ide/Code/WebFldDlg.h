#ifndef _WEB_FLD_DLG_H_
#define _WEB_FLD_DLG_H_

//////////////////////////////////////////////////////////////////////////////////
class WebFldDlg : public LDialog
{
public:
	char *Name;
	LString Ftp;
	char *Www;

	WebFldDlg(LViewI *p, char *name, char *ftp, char *www);
	~WebFldDlg();
	int OnNotify(LViewI *v, int f);
};

#endif