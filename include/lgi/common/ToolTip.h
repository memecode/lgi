#ifndef _GTOOLTIP_H_
#define _GTOOLTIP_H_

/// Puts a tool tip on screen when the mouse wanders over a region.
class LgiClass LToolTip : public LView
{
	class LToolTipPrivate *d;

public:
	LToolTip();
	~LToolTip();

	/// Create a tip
	/// \returns the id of the tip or 0 on failure.
	int NewTip
	(
		/// The text to display
		const char *Name,
		/// The region the mouse has to be in to trigger the tip
		LRect &Pos
	);
	
	/// Delete the tip.
	void DeleteTip(int Id);

	bool Attach(LViewI *p);
};

#endif