#ifndef _GGRAPH_H_
#define _GGRAPH_H_

#include "lgi/common/Db.h"
#include "lgi/common/Layout.h"

class LGraph : public LLayout
{
	struct LGraphPriv *d;

public:
    enum Style
    {
        LineGraph,
        PointGraph,
    };

    struct GGraphPair
    {
	    LVariant x, y;
	    void *UserData;
    };
    
	LGraph(int Id, int XAxix = -1, int YAxis = -1);
	~LGraph();

	// Api
	bool SetDataSource(GDbRecordset *Rs, int XAxis = -1, int YAxis = -1);
	bool AddPair(char *x, char *y, void *UserData = 0);
	void SetStyle(Style s);
	Style GetStyle();
	LArray<GGraphPair*> *GetSelection();

    // Impl
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);	
};

#endif