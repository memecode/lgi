#include "Lgi.h"
#include "Store3.h"

GString EmailFlagsToStr(int flags)
{
	if (flags == 0)
		return "(none)";
	GString::Array a;
	a.SetFixedLength(false);
	#define _(s) if (flags & s) a.New() = #s + 5;
	_(MAIL_SENT)
	_(MAIL_RECEIVED)
	_(MAIL_CREATED)
	_(MAIL_FORWARDED)
	_(MAIL_REPLIED)
	_(MAIL_ATTACHMENTS)
	_(MAIL_READ)
	_(MAIL_READY_TO_SEND)
	_(MAIL_READ_RECEIPT)
	_(MAIL_IGNORE)
	_(MAIL_FIXED_WIDTH_FONT)
	_(MAIL_BOUNCED)
	_(MAIL_BOUNCE)
	_(MAIL_SHOW_IMAGES)
	_(MAIL_BAYES_HAM)
	_(MAIL_BAYES_SPAM)
	_(MAIL_NEW)
	_(MAIL_STORED_FLAT)
	return GString(",").Join(a);
}
