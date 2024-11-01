#pragma once

#include "lgi/common/List.h"
#include "lgi/common/ListItemCheckBox.h"

struct BreakPoint
{
	// User has enabled the break point in the UI
	bool Enabled = true;

	// Use File:Line
		LString File;
		ssize_t Line = 0;
	// -or-
		// A symbol reference
		LString Symbol;
		
	BreakPoint()
	{
	}

	BreakPoint(const char *sym)
	{
		Symbol = sym;
	}
		
	BreakPoint(const char *file, ssize_t line)
	{
		File = file;
		Line = line;
	}
		
	operator bool() const
	{
		if (File && Line > 0)
			return true;
		if (Symbol)
			return true;
		return false;
	}

	BreakPoint &operator =(const BreakPoint &b)
	{
		Enabled = b.Enabled;

		File = b.File;
		Line = b.Line;
			
		Symbol = b.Symbol;
			
		return *this;
	}
		
	bool operator ==(const BreakPoint &b)
	{
		if (File == b.File &&
			Line == b.Line &&
			Symbol == b.Symbol)
			return true;
			
		return false;
	}

	void Empty()
	{
		File.Empty();
		Line = 0;
		Symbol.Empty();
	}
		
	LString Save()
	{
		if (File && Line > 0)
			return LString::Fmt("file://%s:" LPrintfSSizeT, File.Get(), Line);
		else if (Symbol)
			return LString::Fmt("symbol://%s", Symbol.Get());
			
		LAssert(!"Invalid breakpoint");
		return LString();
	}
		
	bool Load(LString s)
	{
		auto sep = s.Find("://");
		if (sep < 0)
			return false;
		auto var = s(0, sep);
		auto value = s(sep+3, -1);
		if (var.Equals("file"))
		{
			auto p = value.SplitDelimit(":");
			LAssert(p.Length() == 2);
			File = p[0];
			Line = p[1].Int();
		}
		else if (var.Equals("symbol"))
		{
			Symbol = value;
		}
		else
		{
			LAssert(!"Invalid type");
			return false;
		}
			
		return true;			
	}
};


class BreakPointStore : public LMutex
{
public:
	constexpr static int INVALID_ID = -1;

	enum TEvent
	{
		TStoreDeleted,
		TBreakPointAdded,
		TBreakPointDeleted,
		TBreakPointEnabled,
		TBreakPointDisabled,
	};

	using TStatusCb = std::function<void(bool)>;
	using TCallback = std::function<void(TEvent,int)>;
	using TLock = LMutex::Auto;

private:
	LList *ui = nullptr;
	
	struct Bp
	{
		BreakPoint obj;
	};

	LHashTbl<IntKey<int>, Bp*> BreakPoints;
	LHashTbl<IntKey<int>, TCallback*> Callbacks;

	template<typename T>
	int AllocId(T &tbl)
	{
		TLock lck(this, _FL);
		while (true)
		{
			auto t = LRand(9999)+1;
			if (!tbl.Find(t))
				return t;
		}
	}	

	struct BpItem : public LListItem
	{
		BreakPointStore *store;
		int id;
		BreakPoint bp;
		LString cache;
		LListItemCheckBox *chk;

		enum TColumn {
			TName,
			TEnable,
		};

		BpItem(BreakPointStore *Store, int Id, BreakPoint Bp) :
			store(Store),
			id(Id),
			bp(Bp),
			chk(new LListItemCheckBox(this, TEnable, true))
		{
		}

		const char *GetText(int col) override
		{
			switch (col)
			{
				case TName:
					cache = bp.Save();
					return cache;
			}

			return nullptr;
		}

		void OnColumnNotify(int Col, int64 Data) override
		{
			store->SetEnable(id, Data != 0);
			LListItem::OnColumnNotify(Col, Data);
		}
	};

public:
	BreakPointStore() : LMutex("BreakPointStore.Lock")
	{
	}

	~BreakPointStore()
	{
		Empty();
		Notify(TStoreDeleted, INVALID_ID);

		{
			TLock lck(this, _FL);
			Callbacks.DeleteObjects();
		}
	}

	void SetUi(LList *lst)
	{
		ui = lst;

		if (ui)
		{
			List<LListItem> items;

			TLock lck(this, _FL);
			for (auto p: BreakPoints)
				items.Insert(new BpItem(this, p.key, p.value->obj));

			ui->Insert(items);
		}
	}

	int AddCallback(TCallback cb)
	{
		TLock lck(this, _FL);

		int id = INVALID_ID;
		if (auto callback = new TCallback(std::move(cb)))
		{
			id = AllocId(Callbacks);
			Callbacks.Add(id, callback);
		}
		return id;
	}

	bool DeleteCallback(int id)
	{
		TLock lck(this, _FL);
		if (auto cb = Callbacks.Find(id))
		{
			delete cb;
			Callbacks.Delete(id);
			return true;
		}		
		return false;
	}

	void Notify(TEvent type, int id)
	{
		TLock lck(this, _FL);

		if (ui)
		{
			// Make sure the UI is up to date:
			switch (type)
			{
				case TBreakPointAdded:
				{
					// Create a new UI item for the breakpoint
					auto bp = Get(id);
					ui->Insert(new BpItem(this, id, bp));
					break;
				}
				case TBreakPointDeleted:
				{
					// Find and delete the BpItem by id
					LArray<BpItem*> all;
					if (ui->GetAll(all))
					{
						for (auto i: all)
						{
							if (i->id == id)
							{
								delete i;
								break;
							}
						}
					}
					break;
				}
				default: break;
			}
		}

		// Call all the callbacks:
		for (auto p: Callbacks)
			(*p.value)(type, id);
	}

	int Add(BreakPoint &breakPoint)
	{
		TLock lck(this, _FL);
		if (auto bp = new Bp)
		{
			auto id = AllocId(BreakPoints);
			bp->obj = breakPoint;
			BreakPoints.Add(id, bp);

			Notify(TBreakPointAdded, id);
			return id;
		}
		return INVALID_ID;
	}

	bool Delete(int id)
	{
		TLock lck(this, _FL);
		if (auto bp = BreakPoints.Find(id))
		{
			delete bp;
			BreakPoints.Delete(id);

			Notify(TBreakPointDeleted, id);
			return true;
		}

		LAssert(!"bp doesn't exist!?");
		return false;
	}

	void SetEnable(int id, bool enabled)
	{
		TLock lck(this, _FL);
		if (auto i = BreakPoints.Find(id))
		{
			if (i->obj.Enabled ^ enabled)
			{
				i->obj.Enabled = enabled;
				Notify(enabled ? TBreakPointEnabled : TBreakPointDisabled, id);
			}
		}
	}

	/// Removes all the break points
	void Empty()
	{
		TLock lck(this, _FL);

		for (auto p: BreakPoints)
			Notify(TBreakPointDeleted, p.key);

		BreakPoints.DeleteObjects();
	}

	/// Returns the number of stored break points
	ssize_t Length()
	{
		return BreakPoints.Length();
	}

	/// Return the id's of all the breakpoints
	LArray<int> GetAll()
	{
		LArray<int> a;
		a.SetFixedLength(false);

		TLock lck(this, _FL);
		for (auto p: BreakPoints)
			a.Add(p.key);

		return a;
	}

	/// Gets a specific breakpoint object
	BreakPoint Get(int id)
	{
		BreakPoint bp;
		TLock lck(this, _FL);
		if (auto i = BreakPoints.Find(id))
			bp = i->obj;
		return bp;
	}

	/// Looks up the id for a given breakpoint
	int Has(BreakPoint &bp)
	{
		TLock lck(this, _FL);
		for (auto p: BreakPoints)
		{
			if (bp == p.value->obj)
				return p.key;
		}

		return INVALID_ID;
	}
};
