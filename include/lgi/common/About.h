/// \file
/// \author Mathew Allen (fret@memecode.com)

#ifndef __GABOUT_H
#define __GABOUT_H

#include "lgi/common/DocView.h"

/// A simple about dialog
class LAbout : public LDialog, public GDefaultDocumentEnv
{
public:
	/// Constructor
	LAbout
	(
		/// The parent window
		LView *parent,
		/// The application name
		const char *AppName,
		/// The version
		const char *Ver,
		/// The description of the application
		const char *Text,
		/// The filename of a graphic to display
		const char *AboutGraphic,
		/// URL for the app
		const char *Url,
		/// Support email addr for the app
		const char *Email
	);

	int OnNotify(LViewI *Ctrl, LNotification n);
};

#endif
