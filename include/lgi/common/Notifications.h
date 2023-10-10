#pragma once

enum LNotifyType
{
	LNotifyNull,

	// LItemContainer notification flags
	/// Item inserted
	/// \sa LList, LView::OnNotify
	LNotifyItemInsert = 0x100,
	/// Item deleted
	/// \sa LList, LView::OnNotify
	LNotifyItemDelete,
	/// Item selected
	/// \sa LList, LView::OnNotify
	LNotifyItemSelect,
	/// Item clicked
	/// \sa LList, LView::OnNotify
	LNotifyItemClick,
	/// Item double clicked
	/// \sa LList, LView::OnNotify
	LNotifyItemDoubleClick,
	/// Item changed
	/// \sa LList, LView::OnNotify
	LNotifyItemChange,
	/// Column changed
	/// \sa LList, LView::OnNotify
	LNotifyItemColumnsChanged,
	/// Column sized
	/// \sa LList, LView::OnNotify
	LNotifyItemColumnsResized,
	/// Column clicks
	/// \sa LList, LView::OnNotify
	LNotifyItemColumnClicked,
	/// Items dropped on the control
	LNotifyItemItemsDropped,
	/// Sent when the control requests a context menu 
	/// outside of the existing items, i.e. in the blank
	/// space below the items.
	LNotifyItemContextMenu,
	/// Blank space clicked
	LNotifyContainerClick,
	/// Ctrl+F - find
	LNotifyContainerFind,

	// Generic value changed
	LNotifyValueChanged,

	// Control notifications
	LNotifyActivate,					// User has used say the keyboard to activate control:
										//		Check box -> toggle value
										//		Radio button -> select
										//		Button -> push or toggle
										//		Text label -> activate "next" control
										//		Most others -> take focus
										// etc

	// LDocView
	LNotifyDocChanged,
	LNotifyDocLoaded,
	LNotifyCursorChanged,
	LNotifySelectionChanged,
	LNotifyCharsetChanged,
	LNotifyFixedWidthChanged,
	LNotifyShowImagesChanged,

	// LTableLayout
	LNotifyTableLayoutChanged,	// Sent by LTableLayout to notify it's layout changed
	LNotifyTableLayoutRefresh,			// Sent by child views of LTableLayout to cause it to
										// update the layout

	// LZoomView
	LNotifyViewportChanged,

	// LTabPage
	LNotifyTabPageButtonClick,
	
	// LScrollBar
	LNotifyScrollBarCreate,
	LNotifyScrollBarDestroy,

	// Popup
	LNotifyPopupDelete,
	LNotifyPopupVisible,
	LNotifyPopupHide,

	/// Return/Enter pressed
	/// \sa LList, LView::OnNotify
	LNotifyReturnKey = LK_RETURN,
	/// Backspace pressed
	/// \sa LList, LView::OnNotify
	LNotifyBackspaceKey = LK_BACKSPACE,
	/// Delete pressed
	/// \sa LList, LView::OnNotify
	LNotifyDeleteKey = LK_DELETE,
	/// Escape pressed
	/// \sa LList, LView::OnNotify
	LNotifyEscapeKey = LK_ESCAPE,
	/// Generic LKey event
	LNotifyLKey,
	
	/// User app notification IDs should start with this value:
	LNotifyUserApp = 0x10000
	/*
	e.g.
	enum MyAppNotifications
	{
		NotifyStart = LNotifyUserApp,
		NotifyEnd,
		NotifyThreadDone,
	};
	*/
};

struct LNotification
{
	constexpr static int MaxInts = 4;

	LNotifyType Type;
	int64_t Int[MaxInts] = {0};
	LString::Array Str;
	LString Sender;

	LNotification(LNotifyType type = LNotifyValueChanged, const char *file = NULL, int line = 0) :
		Type(type)
	{
		if (file && line)
			SetSender(file, line);
	}
	
	LNotification(const LNotification &n)
	{
		Type = n.Type;
		for (int i=0; i<MaxInts; i++)
			Int[i] = n.Int[i];
		Str = n.Str;
		Sender = n.Sender;
	}

	void SetSender(const char *file, int line)
	{
		Sender.Printf("%s:%i", file, line);
	}
	
	// Mouse event encapsulation
	enum MouseParam { MsFlags, MsX, MsY };

	bool IsMouseEvent() const
	{
		return	Type == LNotifyItemClick ||
				Type == LNotifyItemDoubleClick ||
				Type == LNotifyContainerClick ||
				Type == LNotifyItemContextMenu;
	}

	LNotification(const LMouse &m, LNotifyType type = LNotifyNull)
	{
		Type = type ? type : (m.Double() ? LNotifyItemDoubleClick : LNotifyItemClick);
		Int[MsFlags] = m.Flags;
		Int[MsX] = m.x;
		Int[MsY] = m.y;
	}

	LMouse GetMouseEvent() const
	{
		LMouse m;

		if (IsMouseEvent())
		{
			m.Flags = (int)Int[MsFlags];
			m.x = (int)Int[MsX];
			m.y = (int)Int[MsY];
		}
		else LAssert(!"Not a mouse event.");

		return m;
	}
	
	// Keyboard event encapsulation
	enum KeyParam { KeyFlags, KeyVkey, KeyChar, KeyIsChar };

	bool IsKeyEvent() const
	{
		return	Type == LNotifyReturnKey || Type == LNotifyBackspaceKey ||
				Type == LNotifyDeleteKey || Type == LNotifyEscapeKey ||
				Type == LNotifyLKey;
	}

	LNotification(const LKey &k)
	{
		switch (k.vkey)
		{
			case LK_RETURN:
			#if defined(LK_KEYPADENTER) || defined(LGI_GTK)
			case LK_KEYPADENTER:
			#endif
							   Type = LNotifyReturnKey;    break;
			case LK_BACKSPACE: Type = LNotifyBackspaceKey; break;
			case LK_DELETE:    Type = LNotifyDeleteKey;    break;
			case LK_ESCAPE:    Type = LNotifyEscapeKey;    break;
			default:           Type = LNotifyLKey;         break;
		}
		Int[KeyFlags] = k.Flags;
		Int[KeyVkey] = k.vkey;
		Int[KeyChar] = k.c16;
		Int[KeyIsChar] = k.IsChar;
	}

	LKey GetKeyEvent() const
	{
		LKey k;

		if (IsKeyEvent())
		{
			k.Flags = (int)Int[KeyFlags];
			k.vkey = (char16)Int[KeyVkey];
			k.c16 = (char16)Int[KeyChar];
			k.IsChar = Int[KeyIsChar] != 0;
		}
		else LAssert(!"Not a key event.");

		return k;
	}
};
