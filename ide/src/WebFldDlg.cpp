#include "lgi/common/Lgi.h"
#include "lgi/common/Net.h"
#include "lgi/common/Uri.h"

#include "WebFldDlg.h"
#include "resdefs.h"

//////////////////////////////////////////////////////////////////////////////////
WebFldDlg::WebFldDlg(LViewI *p, char *name, char *ftp, char *www)
{
	Name = 0;
	Www = 0;
	SetParent(p);
	LoadFromResource(IDD_WEB_FOLDER);

	if (ftp)
	{
		LUri u(ftp);
		SetCtrlName(IDC_HOST, u.sHost);
		SetCtrlName(IDC_USERNAME, u.sUser);
		SetCtrlName(IDC_PASSWORD, u.sPass);
		SetCtrlName(IDC_PATH, u.sPath);
	}

	SetCtrlName(IDC_NAME, name);
	SetCtrlName(IDC_WWW, www);
	MoveToCenter();
}

WebFldDlg::~WebFldDlg()
{
	DeleteArray(Name);
	DeleteArray(Www);
}

int WebFldDlg::OnNotify(LViewI *v, LNotification &n)
{
	switch (v->GetId())
	{
		case IDOK:
		{
			LUri u;
			u.sHost = GetCtrlName(IDC_HOST);
			u.sUser = GetCtrlName(IDC_USERNAME);
			u.sPass = GetCtrlName(IDC_PASSWORD);
			u.sPath = GetCtrlName(IDC_PATH);
			u.sProtocol = "ftp";
			Ftp = u.ToString();
			Www = NewStr(GetCtrlName(IDC_WWW));
			Name = NewStr(GetCtrlName(IDC_NAME));
			EndModal(1);
			break;
		}
		case IDCANCEL:
		{
			EndModal(0);
			break;
		}
	}

	return 0;
}
