#ifndef _GREGION_CLIP_H_
#define _GREGION_CLIP_H_

class LRegionClipDC : public LSurface
{
	LRegion c;
	LSurface *s;
	
public:
	LRegionClipDC(LSurface *Surface, LRegion *Rgn = NULL)
	{
		s = Surface;
		pMem = s->pMem;
		if (Rgn)
			c = *Rgn;
	}
	
	~LRegionClipDC()
	{
		pMem = NULL;
	}
	
	void SetClipRegion(LRegion *Rgn)
	{
		if (Rgn)
			c = *Rgn;
		else
			c.Empty();
	}
};

#endif