#ifndef __DROP_FILES__
#define __DROP_FILES__

#include "GVariant.h"

class GDropFiles : public GArray<char*>
{
public:
	GDropFiles(GDragData &dd);
    GDropFiles(GVariant &v);
    ~GDropFiles();

	void Init(GVariant &v);
};

class GDropStreams : public GArray<GStream*>
{
public:
	GDropStreams(GDragData &dd);
	~GDropStreams();
};

#endif
