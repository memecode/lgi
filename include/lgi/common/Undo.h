#ifndef __GUNDO_H__
#define __GUNDO_H__

class LUndoEvent
{
public:
    virtual ~LUndoEvent() {}
    virtual void ApplyChange() {}
    virtual void RemoveChange() {}
};

class LUndo
{
	unsigned Pos;
	List<LUndoEvent> Events;

public:
	LUndo() { Pos = 0; }
	~LUndo() { Empty(); }

	int GetPos() { return Pos; }
	bool CanUndo() { return (Pos > 0) && (Events.Length() > 0); }
	bool CanRedo() { return (Pos < Events.Length()) && (Events.Length() > 0); }
	void Empty() { Events.DeleteObjects(); Pos = 0; }

	LUndo &operator +=(LUndoEvent *e)
	{
		while (Events.Length() > Pos)
		{
			auto It = Events.rbegin();
			LUndoEvent *u = *It;
			Events.Delete(It);
			DeleteObj(u);
		}

		Events.Insert(e);
		Pos++;
		return *this;
	}

	void Undo()
	{
		if (CanUndo())
		{
			LUndoEvent *e = Events.ItemAt(Pos-1);
			if (e)
			{
				e->RemoveChange();
				Pos--;
			}
		}
	}

	void Redo()
	{
		if (CanRedo())
		{
			LUndoEvent *e = Events.ItemAt(Pos);
			if (e)
			{
				e->ApplyChange();
				Pos++;
			}
		}
	}
};

#endif
