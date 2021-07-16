#pragma once 

#include "lgi/common/Db.h"
#include "lgi/common/Layout.h"

class LGraph : public LLayout
{
	struct LGraphPriv *d;

public:
	constexpr static int AUTO_AXIS = -1;

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
    
	LGraph(	/// Control identifier
			int Id,
			/// Index into the data source of the X axis value
			int XAxis = AUTO_AXIS,
			/// Index into the data source of the Y axis value
			int YAxis = AUTO_AXIS);
	~LGraph();

	// Api
	bool SetDataSource(	/// Source of records
						LDbRecordset *Rs,
						/// Index into the data source of the X axis value
						int XAxis = AUTO_AXIS,
						/// Index into the data source of the Y axis value
						int YAxis = AUTO_AXIS);
	bool AddPair(char *x, char *y, void *UserData = 0);
	void SetStyle(Style s);
	Style GetStyle();
	LArray<GGraphPair*> *GetSelection();
	bool ShowCursor();
	void ShowCursor(bool show);

    // Impl
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
};
