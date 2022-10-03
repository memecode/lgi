#ifndef _GMIDI_H_
#define _GMIDI_H_

#if defined(WINDOWS)
	#if _MSC_VER >= 1400
	typedef DWORD_PTR MIDI_TYPE;
	#else
	typedef DWORD MIDI_TYPE;
	#endif
    #include "mmsystem.h"
#elif defined(MAC)
    #include <CoreMidi/MIDIServices.h>
#endif

#define MIDI_SYSEX_START	0xf0
#define MIDI_SYSEX_END		0xf7

class LMidi : public LMutex
{
	struct LMidiPriv *d;

	void OnError(char *Func, LString *Error, uint32_t Code, char *File, int Line);

	#if defined WIN32
	friend class LMidiNotifyWnd;
	friend void CALLBACK MidiInProc(HMIDIIN hmi, UINT wMsg, MIDI_TYPE dwInstance, MIDI_TYPE dwParam1, MIDI_TYPE dwParam2);
	void StoreMidi(uint8_t *ptr, int len);
	void ParseMidi();
	#endif

protected:
	// Lock the object before accessing this
	LArray<uint8_t> MidiIn;

	// Arrays of device names
	LString::Array In, Out;
    
    #ifdef MAC
    friend void MidiNotify(const MIDINotification *message, void *refCon);
	friend void MidiRead(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon);
    virtual void OnMidiNotify(const MIDINotification *message);
    #endif

public:	
	LMidi();
	~LMidi();

	static int GetMidiPacketSize(uint8_t *ptr, size_t len);
	
	virtual LStream *GetLog() { return NULL; }

	bool IsMidiOpen();
	bool Connect(int InIdx, int OutIdx, LString *ErrorMsg = NULL);
	void SendMidi(uint8_t *ptr, size_t len, bool quiet);
	void CloseMidi();

	virtual void OnMidiIn(uint8_t *midi, size_t len);
	virtual void OnMidiOut(uint8_t *midi, size_t len);
};

#endif