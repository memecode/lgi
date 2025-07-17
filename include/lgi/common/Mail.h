/**		\file
		\author Matthew Allen
*/

#ifndef __MAIL_H
#define __MAIL_H

#include <functional>

#include "lgi/common/Net.h"
#include "lgi/common/Base64.h"
#include "lgi/common/Progress.h"
#include "lgi/common/Variant.h"
#include "lgi/common/OAuth2.h"
#include "lgi/common/Store3Defs.h"	

#ifndef GPL_COMPATIBLE
#define GPL_COMPATIBLE						0
#endif

// Defines
#define MAX_LINE_SIZE						1024
#define MAX_NAME_SIZE						64
#define EMAIL_LINE_SIZE						76

// #define IsDigit(c) ((c) >= '0' AND (c) <= '9')

// Mail logging defines
#define MAIL_SEND_COLOUR					Rgb24(0, 0, 0xff)
#define MAIL_RECEIVE_COLOUR					Rgb24(0, 0x8f, 0)
#define MAIL_ERROR_COLOUR					Rgb24(0xff, 0, 0)
#define MAIL_WARNING_COLOUR					Rgb24(0xff, 0x7f, 0)
#define MAIL_INFO_COLOUR					Rgb24(0, 0, 0)

// Helper functions
extern void TokeniseStrList(const char *Str, LString::Array &Output, const char *Delim);
extern char ConvHexToBin(char c);
#define ConvBinToHex(i) (((i)<10)?'0'+(i):'A'+(i)-10)
extern void DecodeAddrName(const char *Start, std::function<void(LString,LString)> cb, const char *DefaultDomain);
extern void DecodeAddrName(const char *Start, LAutoString &Name, LAutoString &Addr, const char *DefaultDomain);
extern void DecodeAddrName(const char *Start, LString &Name, LString &Addr, const char *DefaultDomain);
extern int MaxLineLen(char *Text);
extern char *EncodeImapString(const char *s);
extern char *DecodeImapString(const char *s);
extern bool UnBase64Str(LString &s);
extern bool Base64Str(LString &s);

extern const char *sTextPlain;
extern const char *sTextHtml;
extern const char *sTextXml;
extern const char *sApplicationInternetExplorer;
extern const char *sApplicationPgpSignature;
extern const char *sApplicationPgpEncrypted;
extern const char sMultipartMixed[];
extern const char sMultipartEncrypted[];
extern const char sMultipartSigned[];
extern const char sMultipartAlternative[];
extern const char sMultipartRelated[];
extern const char sAppOctetStream[];

// Classes
class MailProtocol;

struct MailProtocolError
{
	int Code = 0;
	LString ErrMsg;
};

class MailProtocolProgress
{
public:
	uint64 Start = 0;
	ssize_t Value = 0;
	ssize_t Range = 0;

	MailProtocolProgress()
	{
	}

	void Empty()
	{
		Start = 0;
		Value = 0;
		Range = 0;
	}
	
	void StartTransfer(ssize_t Size)
	{
		Start = LCurrentTime();
		Value = 0;
		Range = Size;
	}
};

class LogEntry
{
	LColour c;

public:
	LArray<char16> Txt;
	LogEntry(LColour col);

	LColour GetColour() { return c; }
	bool Add(const char *t, ssize_t len = -1);
};

/// Attachment descriptor
class FileDescriptor : public LBase
{
protected:
	// Global
	int64 Size;
	char *MimeType;
	char *ContentId;

	// Read from file
	LFile File;
	LStreamI *Embeded;
	bool OwnEmbeded;
	int64 Offset;
	LMutex *Lock;

	// Write to memory
	uchar *Data;
	LAutoPtr<LStream> DataStream;

public:
	FileDescriptor(LStreamI *embed, int64 Offset, int64 Size, char *Name);
	FileDescriptor(char *name);
	FileDescriptor(char *data, int64 len);
	FileDescriptor();
	~FileDescriptor();

	void SetLock(LMutex *l);
	LMutex *GetLock();
	void SetOwnEmbeded(bool i);

	// Access functions
	LStreamI *GotoObject();		// Get data to read
	uchar *GetData();			// Get data from write
	int Sizeof();
	char *GetMimeType() { return MimeType; }
	void SetMimeType(char *s) { DeleteArray(MimeType); MimeType = NewStr(s); }
	char *GetContentId() { return ContentId; }
	void SetContentId(char *s) { DeleteArray(ContentId); ContentId = NewStr(s); }

	// Decode MIME data to memory
	bool Decode(char *ContentType,
				char *ContentTransferEncoding,
				char *MimeData,
				int MimeDataLength);
};

/// Address descriptor
class AddressDescriptor
{
public:
	uint8_t Status = false;
	EmailAddressType CC = MAIL_ADDR_TO;
	LString sName;
	LString sAddr;
	
	AddressDescriptor(const AddressDescriptor *Copy = NULL);
	virtual ~AddressDescriptor();
	
	void _Delete();
	LString Print();
};

/// Base class for mail protocol implementations
class MailProtocol
{
protected:
	char Buffer[4<<10] = "";
	LMutex SocketLock;
	LAutoPtr<LSocketI> Socket;
	LOAuth2::Params OAuth2;
	LDom *SettingStore = NULL;

	bool Error(const char *file, int line, const char *msg, ...);
	bool Read();
	bool Write(const char *Buf = NULL, bool Log = false);
	
	virtual void OnUserMessage(char *Str) {}

public:
	// Logging
	LStreamI *Logger = NULL;
	void Log(const char *Str, LSocketI::SocketMsgType type);

	// Task Progress
	MailProtocolProgress *Items = NULL;
	MailProtocolProgress *Transfer = NULL;

	// Settings
	int ErrMsgId = 0; /// \sa #L_ERROR_ESMTP_NO_AUTHS, #L_ERROR_ESMTP_UNSUPPORTED_AUTHS
	LString ErrMsgFmt; /// The format for the printf
	LString ErrMsgParam; /// The arguments for the printf
	
	LString ProgramName;
	LString ExtraOutgoingHeaders;
	LString::Array CharsetPrefs;

	// Object
	MailProtocol();
	virtual ~MailProtocol();

	// Methods
	void SetOAuthParams(LOAuth2::Params &p) { OAuth2 = p; }
	void SetSettingStore(LDom *store) { SettingStore = store; }
	
	/// Thread safe hard close (quit now)
	bool CloseSocket()
	{
		LMutex::Auto l(&SocketLock, _FL);

		if (Socket != NULL)
			return Socket->Close() != 0;

		return false;
	}

	void SetError(int ResourceId, const char *Fmt, const char *Param = NULL)
	{
		ErrMsgId = ResourceId;
		ErrMsgFmt = Fmt;
		ErrMsgParam = Param;
	}
};

/////////////////////////////////////////////////////////////////////
// Mail IO parent classes

/// Enable STARTTLS support (requires an SSL capable socket)
#define MAIL_USE_STARTTLS			0x01
/// Use authentication
#define MAIL_USE_AUTH				0x02
/// Force the use of PLAIN type authentication
#define MAIL_USE_PLAIN				0x04
/// Force the use of LOGIN type authentication
#define MAIL_USE_LOGIN				0x08
/// Force the use of NTLM type authentication
#define MAIL_USE_NTLM				0x10
/// Secure auth
#define MAIL_SECURE_AUTH			0x20
/// Use SSL
#define MAIL_SSL					0x40
/// OAUTH2
#define MAIL_USE_OAUTH2				0x80
/// CRAM-MD5
#define MAIL_USE_CRAM_MD5			0x100

/// Mail sending protocol
class MailSink : public MailProtocol
{
public:
	/// Connection setup/shutdown
	virtual bool Open
	(
		/// The transport layer to use
		LSocketI *S,
		/// The host to connect to
		const char *RemoteHost,
		/// The local domain
		const char *LocalDomain,
		/// The sink username (or NULL)
		const char *UserName,
		/// The sink password (or NULL)
		const char *Password,
		/// The port to connect with or 0 for default.
		int Port,
		/// Options: Use any of #MAIL_SSL, #MAIL_USE_STARTTLS, #MAIL_SECURE_AUTH, #MAIL_USE_PLAIN, #MAIL_USE_LOGIN etc or'd together.
		int Flags
	) = 0;
	/// Close the connection
	virtual bool Close() = 0;

	// Commands available while connected

	/// Write the email's contents into the LStringPipe returned from
	/// SendStart and then call SendEnd to finish the transaction
	virtual LStringPipe *SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = NULL) = 0;
	
	/// Finishes the mail send
	virtual bool SendEnd(LStringPipe *Sink) = 0;
};

struct ImapMailFlags
{
	union
	{
		struct
		{
			uint8_t ImapAnswered : 1;
			uint8_t ImapDeleted : 1;
			uint8_t ImapDraft : 1;
			uint8_t ImapFlagged : 1;
			uint8_t ImapRecent : 1;
			uint8_t ImapSeen : 1;
			uint8_t ImapExpunged :1;
		};
		uint16 All = 0;
	};

	ImapMailFlags(const char *init = NULL)
	{
		if (init)
			Set(init);
	}

	LString Get()
	{
		char s[256] = "";
		int ch = 0;

		if (ImapAnswered) ch += sprintf_s(s+ch, sizeof(s)-ch, "\\answered ");
		if (ImapDeleted)  ch += sprintf_s(s+ch, sizeof(s)-ch, "\\deleted ");
		if (ImapDraft)    ch += sprintf_s(s+ch, sizeof(s)-ch, "\\draft ");
		if (ImapFlagged)  ch += sprintf_s(s+ch, sizeof(s)-ch, "\\flagged ");
		if (ImapRecent)   ch += sprintf_s(s+ch, sizeof(s)-ch, "\\recent ");
		if (ImapSeen)     ch += sprintf_s(s+ch, sizeof(s)-ch, "\\seen ");

		if (ch == 0)
			return LString();

		LAssert(ch < sizeof(s));
		s[--ch] = 0;
		return s;
	}

	void Set(const char *s)
	{
		All = 0;

		if (!s) s = "";
		while (*s)
		{
			if (*s == '/' || *s == '\\')
			{
				while (*s == '/' || *s == '\\')
					s++;
				const char *e = s;
				while (*e && isalpha(*e))
					e++;

				if (!_strnicmp(s, "answered", e-s))
					ImapAnswered = true;
				else if (!_strnicmp(s, "deleted", e-s))
					ImapDeleted = true;
				else if (!_strnicmp(s, "draft", e-s))
					ImapDraft = true;
				else if (!_strnicmp(s, "flagged", e-s))
					ImapFlagged = true;
				else if (!_strnicmp(s, "recent", e-s))
					ImapRecent = true;
				else if (!_strnicmp(s, "seen", e-s))
					ImapSeen = true;
				
				s = e;
			}
			else s++;
		}
	}

	ImapMailFlags &operator =(const ImapMailFlags &f)
	{
		All = f.All;
		return *this;
	}

	bool operator ==(ImapMailFlags &f) const
	{
		return All == f.All;
	}

	bool operator !=(ImapMailFlags &f) const
	{
		return All != f.All;
	}
};

/// A bulk mail handling class
class MailTransaction
{
public:
	/// The index of the mail in the folder
	int Index;
	
	/// \sa #MAIL_POSTED_TO_GUI, #MAIL_EXPLICIT
	int Flags;
	
	// bool Delete;
	bool Status;
	bool Oversize;

	/// The mail protocol handler writes the email to this stream
	LStreamI *Stream;

	/// Flags used on the IMAP protocolf
	ImapMailFlags Imap;

	/// The user app can use this for whatever
	void *UserData;

	MailTransaction();
	~MailTransaction();
};

/// Return code from MailSrcCallback
enum MailSrcStatus
{
	/// Download the whole email
	DownloadAll,
	/// Download just the top part
	DownloadTop,
	/// Skip this email
	DownloadNone,
	/// About the whole receive
	DownloadAbort
};

/// The callback function used by MailSource::Receive
typedef MailSrcStatus (*MailSrcCallback)
(
	/// The currently executing transaction
	MailTransaction *Trans,
	/// The size of the email about to be downloaded
	uint64 Size,
	/// If DownloadTop is returned, you can set the number of lines to retreive here
	int *LinesToDownload,
	/// The data cookie passed into MailSource::Receive
	void *Data
);

/// The callback function used by MailSource::Receive
typedef bool (*MailReceivedCallback)
(
	/// The currently executing transaction
	MailTransaction *Trans,
	/// The data cookie passed into MailSource::Receive
	void *Data
);

/// Collection of callbacks called during mail receive. You should zero this
/// entire object before using it. Because if someone adds new callbacks after
/// you write the calling code you wouldn't want to leave some callbacks un-
/// initialized. A NULL callback is ignored.
struct MailCallbacks
{
	/// The callback data
	void *CallbackData;
	/// Called before receiving mail
	MailSrcCallback OnSrc;
	/// Called after mail received
	MailReceivedCallback OnReceive;
};
/// A generic mail source object
class MailSource : public MailProtocol
{
public:
	/// Opens a connection to the server
	virtual bool Open
	(
		/// The transport socket
		LSocketI *S,
		/// The hostname or IP of the server
		const char *RemoteHost,
		/// The port on the host to connect to
		int Port,
		/// The username for authentication
		const char *User,
		/// The password for authentication
		const char *Password,
		/// [Optional] Persistant storage of settings
		LDom *SettingStore,
		/// [Optional] Flags: #MAIL_SOURCE_STARTTLS, #MAIL_SOURCE_AUTH, #MAIL_SOURCE_USE_PLAIN, #MAIL_SOURCE_USE_LOGIN
		int Flags = 0) = 0;
	/// Closes the connection
	virtual bool Close() = 0;

	/// Returns the number of messages available on the server
	virtual ssize_t GetMessages() = 0;
	/// Receives a list of messages from the server.
	virtual bool Receive
	(
		/// An array of messages to receive. The MailTransaction objects contains the index of the message to receive
		/// and various status values returned after the operation.
		LArray<MailTransaction*> &Trans,
		/// An optional set of callback functions.
		MailCallbacks *Callbacks = 0
	) = 0;
	/// Deletes a message on the server
	virtual bool Delete(int Message) = 0;
	/// Gets the size of the message on the server
	virtual int Sizeof(int Message) = 0;
	/// Gets the size of all the messages on the server
	virtual bool GetSizes(LArray<int64_t> &Sizes) { return false; }
	/// Gets the unique identifier of the message
	virtual bool GetUid(int Message, char *Id, int IdLen) = 0;
	/// Gets the unique identifiers of a list of messages
	virtual bool GetUidList(LString::Array &Id) = 0;
	/// Gets the headers associated with a given message
	virtual LString GetHeaders(int Message) = 0;
	/// Sets the proxy server. e.g. HTTP mail.
	virtual void SetProxy(char *Server, int Port) {}
};

/////////////////////////////////////////////////////////////////////
// Mail IO implementations

/// SMTP implementation
class MailSmtp : public MailSink
{
protected:
	bool ReadReply(const char *Str, LStringPipe *Pipe = NULL, MailProtocolError *Err = NULL);
	bool WriteText(const char *Str);

public:
	MailSmtp();
	~MailSmtp();

	bool Open(LSocketI *S, const char *RemoteHost, const char *LocalDomain, const char *UserName, const char *Password, int Port = 0, int Flags = 0);
	bool Close();

	bool SendToFrom(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = NULL);
	LStringPipe *SendData(MailProtocolError *Err = NULL);

	LStringPipe *SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = NULL);
	bool SendEnd(LStringPipe *Sink);

	// bool Send(MailMessage *Msg, bool Mime = false);
};

class MailSendFolder : public MailSink
{
	class MailPostFolderPrivate *d;

public:
	MailSendFolder(char *Path);
	~MailSendFolder();

	bool Open(LSocketI *S, const char *RemoteHost, const char *LocalDomain, const char *UserName, const char *Password, int Port = 0, int Flags = 0);
	bool Close();

	LStringPipe *SendStart(List<AddressDescriptor> &To, AddressDescriptor *From, MailProtocolError *Err = NULL);
	bool SendEnd(LStringPipe *Sink);
};

class MailPop3 : public MailSource
{
protected:
	bool ReadReply();
	LString ReadMultiLineReply();
	int GetInt();
	bool MailIsEnd(LString &s);
	bool ListCmd(const char *Cmd, LHashTbl<ConstStrKey<char,false>, bool> &Results);

	const char *End;
	const char *Marker;
	int Messages;

public:
	MailPop3();
	~MailPop3();

	// Connection
	bool Open(LSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, LDom *SettingStore, int Flags = 0) override;
	bool Close() override;

	// Commands available while connected
	ssize_t GetMessages() override;
	bool Receive(LArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = NULL) override;
	bool Delete(int Message) override;
	int Sizeof(int Message) override;
	bool GetSizes(LArray<int64_t> &Sizes) override;
	bool GetUid(int Message, char *Id, int IdLen) override;
	bool GetUidList(LString::Array &Id) override;
	LString GetHeaders(int Message) override;
};

class MailReceiveFolder : public MailSource
{
protected:
	class MailReceiveFolderPrivate *d;

public:
	MailReceiveFolder(char *Path);
	~MailReceiveFolder();

	// Connection
	bool Open(LSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, LDom *SettingStore, int Flags = 0) override;
	bool Close() override;

	// Commands available while connected
	ssize_t GetMessages() override;
	bool Receive(LArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = NULL) override;
	bool Delete(int Message) override;
	int Sizeof(int Message) override;
	bool GetUid(int Message, char *Id, int IdLen) override;
	bool GetUidList(LString::Array &Id) override;
	LString GetHeaders(int Message) override;
};

class MailPhp : public MailSource
{
protected:
	class MailPhpPrivate *d;

	bool Get(LSocketI *S, char *Uri, LStream &Out, bool ChopDot);

public:
	MailPhp();
	~MailPhp();

	// Connection
	bool Open(LSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, LDom *SettingStore, int Flags = 0) override;
	bool Close() override;

	// Commands available while connected
	ssize_t GetMessages() override;
	bool Receive(LArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = NULL) override;
	bool Delete(int Message) override;
	int Sizeof(int Message) override;
	bool GetSizes(LArray<int64_t> &Sizes) override;
	bool GetUid(int Message, char *Id, int IdLen) override;
	bool GetUidList(LString::Array &Id) override;
	LString GetHeaders(int Message) override;
	void SetProxy(char *Server, int Port) override;
};

class MailImapFolder
{
	friend class MailIMap;
	friend struct ImapThreadPrivate;

	char Sep;
	char *Path;

public:
	bool NoSelect;
	bool NoInferiors;
	bool Marked;
	int Exists;
	int Recent;
	int Deleted;
	// int UnseenIndex;

	MailImapFolder();
	virtual ~MailImapFolder();

	char *GetPath();
	void SetPath(const char *s);
	char *GetName();
	void SetName(const char *s);
	char GetSep() { return Sep; }
	void operator =(LHashTbl<ConstStrKey<char>,int> &v);
};

class MailIMap : public MailSource
{
protected:
	class MailIMapPrivate *d;

	char Buf[2048];
	List<char> Uid;
	LStringPipe ReadBuf;
	List<char> Dialog;

	void ClearDialog();
	void ClearUid();
	bool FillUidList();
	bool WriteBuf(bool ObsurePass = false, const char *Buffer = NULL, bool Continuation = false);
	bool ReadResponse(int Cmd = -1, bool Plus = false);
	bool Read(LStreamI *Out = NULL, int Timeout = -1);
	bool ReadLine();
	bool IsResponse(const char *Buf, int Cmd, bool &Ok);
	void CommandFinished();

public:
	typedef LHashTbl<ConstStrKey<char,false>,LString> StrMap;

	struct StrRange
	{
		ssize_t Start, End;
	
		void Set(ssize_t s, ssize_t e)
		{
			Start = s;
			End = e;
		}

		ssize_t Len() { return End - Start; }
	};

	// Typedefs
	struct Untagged
	{
		LString Cmd;
		LString Param;
		int Id;
	};
	
	/// This callback is used to notify the application using this object of IMAP fetch responses.
	/// \returns true if the application wants to continue reading and has taken ownership of the strings in "Parts".
	typedef bool (*FetchCallback)
	(
		/// The IMAP object
		class MailIMap *Imap,
		/// The message sequence number
		uint32_t Msg,
		/// The fetch parts (which the callee needs to own if returning true)
		StrMap &Parts,
		/// The user data passed to the Fetch function
		void *UserData
	);

	// Object
	MailIMap();
	~MailIMap();

	// Mutex
	bool Lock(const char *file, int line);
	bool LockWithTimeout(int Timeout, const char *file, int line);
	void Unlock();

	// General
	char GetFolderSep();
	char *EncodePath(const char *Path);
	char *GetCurrentPath();
	bool GetExpungeOnExit();
	void SetExpungeOnExit(bool b);
	bool ServerOption(char *Opt);
	bool IsOnline();
	const char *GetWebLoginUri();
	void SetParentWindow(LViewI *wnd);
	void SetCancel(LCancel *Cancel);
	ssize_t ParseImapResponse(char *Buffer, ssize_t BufferLen, LArray<StrRange> &Ranges, int Names);

	// Connection
	bool Open(LSocketI *S, const char *RemoteHost, int Port, const char *User, const char *Password, LDom *SettingStore, int Flags = 0) override;
	bool Close() override; // Non-threadsafe soft close (normal operation)
	bool GetCapabilities(LArray<char*> &s);

	// Commands available while connected
	bool Receive(LArray<MailTransaction*> &Trans, MailCallbacks *Callbacks = NULL) override;
	ssize_t GetMessages() override;
	bool Delete(int Message) override;
	bool Delete(bool ByUid, const char *Seq);
	int Sizeof(int Message) override;
	bool GetSizes(LArray<int64_t> &Sizes) override;
	bool GetUid(int Message, char *Id, int IdLen) override;
	bool GetUidList(LString::Array &Id) override;
	LString GetHeaders(int Message) override;
	char *SequenceToString(LArray<int> *Seq);

	// Imap specific commands

	/// This method wraps the imap FETCH command
	/// \returns the number of records accepted by the callback fn
	int Fetch
	(
		/// True if 'Seq' is a UID, otherwise it's a sequence
		bool ByUid,
		/// The sequence number or UID
		const char *Seq,
		/// The parts to retrieve
		const char *Parts,
		/// Data is returned to the caller via this callback function
		FetchCallback Callback,
		/// A user defined param to pass back to the 'Callback' function.
		void *UserData,
		/// [Optional] The raw data received will be written to this stream if provided, else NULL.
		LStreamI *RawCopy = NULL,
		/// [Optional] The rough size of the fetch... used to pre-allocate a buffer to receive data.
		int64 SizeHint = -1,
		/// [Optional] An error object
		LError *Error = NULL
	);

	/// Appends a message to the specified folder
	bool Append
	(
		/// The folder to write to
		const char *Folder,
		/// [Optional] Flags for the message
		ImapMailFlags *Flags,
		/// The rfc822 body of the message
		const char *Msg,
		/// [Out] The UID of the message appended (if known, can be empty if not known)
		LString &NewUid
	);

	bool GetFolders(LArray<MailImapFolder*> &Folders);
	bool SelectFolder(const char *Path, StrMap *Values = NULL);
	char *GetSelectedFolder();
	int GetMessages(const char *Path);
	bool CreateFolder(MailImapFolder *f);
	bool DeleteFolder(const char *Path);
	bool RenameFolder(const char *From, const char *To);
	bool SetFolderFlags(MailImapFolder *f);
	
	/// Expunges (final delete) any deleted messages the current folder.
	bool ExpungeFolder();
	
	// Uid methods
	bool CopyByUid(LArray<uint32_t> &InUids, const char *DestFolder);
	bool SetFlagsByUid(LArray<uint32_t> &Uids, const char *Flags);

	/// Idle processing...
	/// \returns true if something happened
	bool StartIdle();
	// bool OnIdle(int Timeout, LArray<Untagged> &Resp);
	bool OnIdle(int Timeout, LString::Array &Resp);
	bool FinishIdle();
	bool Poll(int *Recent = NULL, LArray<LString> *New = NULL);
	bool Status(char *Path, int *Recent);
	bool Search(bool Uids, LArray<LString> &SeqNumbers, const char *Filter);

	#if 0
	// Utility
	static bool Http(LSocketI *S,
					LAutoString *OutHeaders,
					LAutoString *OutBody,
					int *StatusCode,
					const char *InMethod,
					const char *InUri,
					const char *InHeaders,
					const char *InBody);
	#endif
};

#endif
