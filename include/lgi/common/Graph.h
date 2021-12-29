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

    struct Pair
    {
	    LVariant x, y;
	    void *UserData;
    };

	struct Range
	{
		LVariant Min, Max;
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
	LArray<Pair*> *GetSelection();
	bool ShowCursor();
	void ShowCursor(bool show);
	const char *GetLabel(bool XAxis);
	void SetLabel(bool XAxis, const char *Label);
	Range GetRange(bool XAxis);
	void SetRange(bool XAxis, Range r);

    // Impl
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
	bool OnMouseWheel(double Lines);
};
