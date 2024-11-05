#ifndef _IMAGE_COMPARE_H_
#define _IMAGE_COMPARE_H_

class ImageCompareDlg : public LWindow
{
	struct ImageCompareDlgPriv *d;

public:
	ImageCompareDlg(LView *p, const char *OutPath);
	~ImageCompareDlg();
	
	int OnNotify(LViewI *Ctrl, const LNotification &n) override;
};

#endif