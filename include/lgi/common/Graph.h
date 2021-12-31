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

	class DataSeries : public LBase
	{
		friend class LGraph;
		struct DataSeriesPriv *d;
		LGraphPriv *priv;

	public:
		DataSeries(LGraphPriv *graphPriv, const char *name);
		~DataSeries();

		LColour GetColour();
		void SetColour(LColour c);
		bool SetDataSource(	/// Source of records
							LDbRecordset *Rs,
							/// Index into the data source of the X axis value
							int XAxis = AUTO_AXIS,
							/// Index into the data source of the Y axis value
							int YAxis = AUTO_AXIS);
		bool AddPair(char *x, char *y, void *UserData = NULL);
	};
    
	LGraph(	/// Control identifier
			int Id,
			/// Index into the data source of the X axis value
			int XAxis = AUTO_AXIS,
			/// Index into the data source of the Y axis value
			int YAxis = AUTO_AXIS);
	~LGraph();

	// Api
	DataSeries *GetData(const char *name, bool create = true);
	DataSeries *GetDataAt(size_t index);
	size_t GetDataLength();

	void SetStyle(Style s);
	Style GetStyle();
	LArray<Pair*> *GetSelection();
	bool ShowCursor();
	void ShowCursor(bool show);
	const char *GetLabel(bool XAxis);
	void SetLabel(bool XAxis, const char *Label);
	Range GetRange(bool XAxis);
	void SetRange(bool XAxis, Range r);

	void Empty();

    // Impl
	void OnPaint(LSurface *pDC);
	void OnMouseClick(LMouse &m);
	void OnMouseMove(LMouse &m);
	bool OnMouseWheel(double Lines);
};
