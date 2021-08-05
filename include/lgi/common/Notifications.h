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
	
	
	/// User app notification IDs should start with this value:
	GNotifyUserApp = 0x10000
	/*
	e.g.
	enum MyAppNotifications
	{
		NotifyStart = GNotifyUserApp,
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

	LNotification(LNotifyType type) : Type(type) {}
};
