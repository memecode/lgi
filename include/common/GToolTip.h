#ifndef _GTOOLTIP_H_
#define _GTOOLTIP_H_

/// Puts a tool tip on screen when the mouse wanders over a region.
class LgiClass GToolTip : public GView
{
	class GToolTipPrivate *d;

public:
	GToolTip();
	~GToolTip();

	/// Create a tip
	int NewTip
	(
		/// The text to display
		char *Name,
		/// The region the mouse has to be in to trigger the tip
		GRect &Pos
	);
	
	/// Delete the tip.
	void DeleteTip(int Id);

	bool Attach(GViewI *p);
};

#endif