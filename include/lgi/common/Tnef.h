#ifndef _TNEF_H_
#define _TNEF_H_

class TnefFileInfo
{
public:
	char *Name;
	int64 Start;
	int64 Size;

	TnefFileInfo()
	{
		Name = 0;
		Start = Size = 0;
	}

	~TnefFileInfo()
	{
		DeleteArray(Name);
	}
};

bool TnefReadIndex(LStreamI *Tnef, GArray<TnefFileInfo*> &Index);
bool TnefExtract(LStreamI *Tnef, LStream *Out, TnefFileInfo *File);

#endif