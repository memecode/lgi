#include "lgi/common/Lgi.h"
#include "lgi/common/Midi.h"

#define MIDI_MIRROR_IN_TO_OUT		0

#ifdef WINDOWS

#define M_MIDI_IN	(M_USER + 2000)
class LMidiNotifyWnd : public LWindow
{
	LMidi *m;
	
public:
	LMidiNotifyWnd(LMidi *midi)
	{
		m = midi;
		Attach(0);
	}
	
	LMessage::Result OnEvent(LMessage *Msg)
	{
		if (Msg->Msg() == M_MIDI_IN)
		{
			m->ParseMidi();
		}
		
		return LView::OnEvent(Msg);
	}
};

#endif

struct LMidiPriv
{
	LMidi *Midi;
	
	#if defined(MAC)
		MIDIClientRef Client = 0;
		MIDIPortRef InPort, OutPort;
		LArray<MIDIDeviceRef> Devs;
		LArray<MIDIEndpointRef> Srcs, Dsts;
		MIDIEndpointRef Dst = 0;
		LArray<uint8_t> Data;
	#elif defined(WINDOWS)
		HMIDIIN hIn = 0;
		HMIDIOUT hOut = 0;
		MIDIHDR InHdr;
		char InBuf[2 << 10];
		LMidiNotifyWnd Notify;
	#elif defined(LINUX)
		int Hnd = -1;
		LAutoPtr<LThread> Reader;
	#endif
	
	LMidiPriv(LMidi *m)
		#ifdef WINDOWS
		: Notify(m)
		#endif
	{
		Midi = m;
	}
};

#if defined(MAC)

	LAutoString PrintMIDIObjectInfo(MIDIObjectRef o)
	{
		CFStringRef pName, pManuf, pModel;
		SInt32 Id;
		MIDIObjectGetStringProperty(o, kMIDIPropertyName, &pName);
		MIDIObjectGetStringProperty(o, kMIDIPropertyManufacturer, &pManuf);
		MIDIObjectGetStringProperty(o, kMIDIPropertyModel, &pModel);
		MIDIObjectGetIntegerProperty(o, kMIDIPropertyUniqueID, &Id);
		LString Name = pName;
		LString Man = pManuf;
		LString Mod = pModel;

		char s[256];
		snprintf(s, sizeof(s), "%s, %s, %s, %x", Name.Get(), Man.Get(), Mod.Get(), (uint32_t)Id);
		printf("%s\n", s);

		CFRelease(pName);
		CFRelease(pManuf);
		CFRelease(pModel);
		
		return LAutoString(NewStr(s));
	}

	void MidiNotify(const MIDINotification *message, void *refCon)
	{
		LMidi *a = (LMidi*)refCon;
		a->OnMidiNotify(message);
	}

	void MidiRead(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon)
	{
		LMidi *a = (LMidi*)readProcRefCon;
		if (a)
		{
			LArray<uint8_t> *d = &a->d->Data;
			
			// Collect all data from the input stream
			MIDIPacket *packet = (MIDIPacket*)pktlist->packet;
			for (unsigned int j = 0; j < pktlist->numPackets; j++)
			{
				for (unsigned i=0; i<packet->length; i++)
					d->Add(packet->data[i]);
				packet = MIDIPacketNext(packet);
			}

			// Parse into full MIDI packets
			int Len;
			while
			(
				(
					Len = a->GetMidiPacketSize( &(*d)[0], d->Length() )
				)
				> 0
			)
			{
				// Send full packet to the app
				a->OnMidiIn(&(*d)[0], Len);
				
				// Remove the message from the buffer
				if (d->Length() > Len)
				{
					auto Remaining = d->Length() - Len;
					if (Remaining > 0)
					{
						memmove(&(*d)[0], &d[Len], Remaining);
						d->Length(Remaining);
					}
				}
				else
				{
					d->Length(0);
					break;
				}
			}
		}
	}

	void LMidi::OnMidiNotify(const MIDINotification *message)
	{
		printf("MIDI notify: %i\n", (int)message->messageID);
	}

#elif defined(WINDOWS)

	void CALLBACK MidiOutProc(HMIDIOUT hmo, UINT wMsg, MIDI_TYPE dwInstance, MIDI_TYPE wParam, MIDI_TYPE lParam)
	{
		LMidi *a = (LMidi*)dwInstance;
		switch (wMsg)
		{
			case MM_MOM_OPEN:
				// ya ya... we're open for business.
				break;
			case MM_MOM_DONE:
			{
				MIDIHDR *Hdr = (MIDIHDR*)wParam;
				if (!Hdr->dwUser)
					a->OnMidiOut((uchar*)Hdr->lpData, Hdr->dwBufferLength);
				// a->GetLog()->Print("MidiOutProc MM_MOM_DONE\n");
				break;
			}
			default:
			{
				if (a->GetLog())
					a->GetLog()->Print("MidiOutProc 0x%x\n", wMsg);
				break;
			}
		}
	}

	void CALLBACK MidiInProc(HMIDIIN hmi, UINT wMsg, MIDI_TYPE dwInstance, MIDI_TYPE dwParam1, MIDI_TYPE dwParam2)
	{
		LMidi *a = (LMidi*)dwInstance;
		uint8_t *b = 0;
		int len = 0;
		uint8_t buf[3];

		// LgiTrace("MidiInProc wMsg=%i\n", wMsg);
		switch (wMsg)
		{
			case MM_MIM_OPEN:
				// ya ya... we're open for business.
				break;
			case MM_MIM_CLOSE:
				break;
			case MM_MIM_DATA:
			{
				buf[0] = (uint8_t) (dwParam2 & 0xff);
				buf[1] = (uint8_t) ((dwParam2 >> 8) & 0xff);
				buf[2] = (uint8_t) ((dwParam2 >> 16) & 0xff);
				b = buf;
				
				switch (b[0] >> 4)
				{
					case 0x8:
					case 0x9:
					case 0xa:
					case 0xb:
					case 0xe:
						len = 3;
						break;
					case 0xc:
					case 0xd:
						len = 2;
						break;
					default:
						b = NULL;
						break;
				}
				break;
			}
			case MM_MIM_LONGDATA:
			{
				MIDIHDR *Hdr = (MIDIHDR*)dwParam1;
				b = (uint8_t*) Hdr->lpData;
				len = Hdr->dwBytesRecorded;
				
				if (len)
					midiInAddBuffer(hmi, Hdr, sizeof(*Hdr));
				break;
			}
			default:
			{
				if (a->GetLog())
					a->GetLog()->Print("MidiInProc 0x%x\n", wMsg);
				break;
			}
		}

		if (b && len > 0)
			a->StoreMidi(b, len);
	}

#elif defined (LINUX)

	#include "errno.h"

	class ReaderThread : public LThread
	{
		LMidiPriv *d;
		bool loop;
		
	public:
		ReaderThread(LMidiPriv *priv) : LThread("MidiReaderThread")
		{
			d = priv;
			loop = true;
			Run();
		}
		
		~ReaderThread()
		{
			loop = false;
			while (!IsExited())
				LSleep(1);
		}
		
		int Main()
		{
			while (d->Hnd >= 0 && loop)
			{
				uint8_t buf[128];
				int rd = read(d->Hnd, buf, sizeof(buf));
				if (rd > 0)
				{
					d->Midi->OnMidiIn(buf, rd);
				}
			}
			
			return 0;
		}
	};

#endif

LMidi::LMidi() : LMutex("LMidi")
{
	d = new LMidiPriv(this);

	#if defined(MAC)

		auto Devs = MIDIGetNumberOfDevices();
		for (int i = 0; i < Devs; i++)
		{
			MIDIDeviceRef dev = MIDIGetDevice(i);
			d->Devs[i] = dev;
			LAutoString a = PrintMIDIObjectInfo(dev);
			In.New() = a;
			Out.New() = a;

			auto nEnt = MIDIDeviceGetNumberOfEntities(dev);
			// printf("Device %d has %d entities:\n", i, nEnt);
			for (int n = 0; n < nEnt; ++n)
			{
				MIDIEntityRef ent = MIDIDeviceGetEntity(dev, n);
				// printf("	 ");
				// PrintMIDIObjectInfo(ent);
				auto nSources = MIDIEntityGetNumberOfSources(ent);
				// printf("	 Entity %d has %d source endpoints:\n", n, nSources);
				for (int k = 0; k < nSources; ++k)
				{
					MIDIEndpointRef sep = MIDIEntityGetSource(ent, k);
					d->Srcs[i] = sep;
					// printf("	   ");
					// PrintMIDIObjectInfo(sep);
				}

				auto nDestinations = MIDIEntityGetNumberOfDestinations(ent);
				// printf("	 Entity %d has %d destination endpoints:\n", i, nDestinations);
				for (int l = 0; l < nDestinations; ++l)
				{
					MIDIEndpointRef dep = MIDIEntityGetDestination(ent, l);
					d->Dsts[i] = dep;
					// printf("	   ");
					// PrintMIDIObjectInfo(dep);
				}
			}
		}
	
	#elif defined(WINDOWS)

		UINT InDevs = midiInGetNumDevs();
		UINT OutDevs = midiOutGetNumDevs();
		UINT n;

		MIDIINCAPS InCaps;
		for (n=0; n<InDevs; n++)
		{
			MMRESULT r = midiInGetDevCaps(n, &InCaps, sizeof(InCaps));
			if (r == MMSYSERR_NOERROR)
				In.New() = InCaps.szPname;
		}

		MIDIOUTCAPS OutCaps;
		for (n=0; n<OutDevs; n++)
		{
			MMRESULT r = midiOutGetDevCaps(n, &OutCaps, sizeof(OutCaps));
			if (r == MMSYSERR_NOERROR)
				Out.New() = OutCaps.szPname;
		}
	
	#elif defined(LINUX)
	
		LDirectory Dir;
		const char *Path = "/dev";
		for (int b = Dir.First(Path); b; b = Dir.Next())
		{
			if (!Dir.IsDir() &&
				stristr(Dir.GetName(), "midi"))
			{
				char Full[128];
				LMakePath(Full, sizeof(Full), Path, Dir.GetName());
				printf("Midi[" LPrintfSizeT "] = %s\n", In.Length(), Full);
				In.New() = Full;
				Out.New() = Full;
			}
		}
	
	#endif
}

LMidi::~LMidi()
{
	CloseMidi();
	delete d;
}

bool LMidi::IsMidiOpen()
{
	#if defined(MAC)
	return d->Client != 0;
	#elif defined(WINDOWS)
	return d->hIn != 0;
	#elif defined(LINUX)
	return d->Hnd >= 0;
	#endif
}

int LMidi::GetMidiPacketSize(uint8_t *ptr, size_t len)
{
	if (!ptr || len < 1)
		return 0;

	if (ptr[0] == MIDI_SYSEX_START)
	{
		int i;
		for (i=1; i<len; i++)
		{
			if (ptr[i] == MIDI_SYSEX_END)
				return i + 1;
		}
	}
	else
	{
		switch (ptr[0] >> 4)
		{
			case 0x8:
			case 0x9:
			case 0xa:
			case 0xb:
			case 0xe:
				if (len >= 3)
					return 3;
				break;
			case 0xc:
			case 0xd:
				if (len >= 2)
					return 2;
				break;
			default:
				return 1;
				break;
		}
	}

	return 0;
}

#ifdef WINDOWS

void LMidi::ParseMidi()
{
	if (Lock(_FL))
	{
		while (MidiIn.Length() > 0)
		{
			auto len = GetMidiPacketSize(&MidiIn[0], MidiIn.Length());
			if (len > 0)
			{
				OnMidiIn(&MidiIn[0], len);
				auto Remaining = MidiIn.Length() - len;
				if (Remaining > 0)
				{
					memmove(&MidiIn[0], &MidiIn[len], Remaining);
					MidiIn.Length(Remaining);
				}
				else
				{
					MidiIn.Length(0);
					break;
				}
			}
			else
			{
				LAssert(0);
				break;
			}
		}
		Unlock();
	}
}

void LMidi::StoreMidi(uint8_t *ptr, int len)
{
	if (Lock(_FL))
	{
		MidiIn.Add(ptr, len);
		Unlock();
		
		#ifdef WINDOWS
		d->Notify.PostEvent(M_MIDI_IN);
		#endif
	}
}

#endif

bool LMidi::Connect(int InIdx, int OutIdx, LString *ErrorMsg)
{
	if (IsMidiOpen())
	{
		if (ErrorMsg)
			*ErrorMsg = "Already connected.";
		return false;
	}

	bool Status = false;
	
	#ifdef MAC
	
		CFStringRef Name = CFSTR("AxeFoot");
		OSStatus e = MIDIClientCreate(Name, MidiNotify, this, &d->Client);
		if (e)
		{
			if (GetLog())
				GetLog()->Print("%s:%i - MIDIClientCreate failed with %i\n", _FL, e);
		}
		else
		{
			e = MIDIInputPortCreate(d->Client, CFSTR("AxeFootInput"), MidiRead, this, &d->InPort);
			if (e)
			{
				if (GetLog())
					GetLog()->Print("%s:%i - MIDIInputPortCreate failed with %i\n", _FL, e);
			}
			else
			{
				e = MIDIOutputPortCreate(d->Client, CFSTR("AxeFootOutput"), &d->OutPort);
				if (e)
				{
					if (GetLog())
						GetLog()->Print("%s:%i - MIDIOutputPortCreate failed with %i\n", _FL, e);
				}
				else
				{
					MIDIEndpointRef Src = d->Srcs[InIdx];
					d->Dst = d->Dsts[OutIdx];

					e = MIDIPortConnectSource(d->InPort, Src, this);
					if (e)
					{
						if (GetLog())
							GetLog()->Print("%s:%i - MIDIPortConnectSource failed with %i\n", _FL, e);
					}
					else
					{
						Status = true;
						if (GetLog())
							GetLog()->Print("Created midi client ok.\n");
					}
				}
			}
		}

	#elif defined(WINDOWS)

		MMRESULT r = midiOutOpen(&d->hOut, OutIdx, (MIDI_TYPE)MidiOutProc, (MIDI_TYPE)this, CALLBACK_FUNCTION);
		if (r != MMSYSERR_NOERROR)
		{
			OnError("midiOutOpen", ErrorMsg, r, _FL);
		}
		else
		{
			r = midiInOpen(&d->hIn, InIdx, (MIDI_TYPE)MidiInProc, (MIDI_TYPE)this, CALLBACK_FUNCTION);
			if (r != MMSYSERR_NOERROR)
			{
				OnError("midiInOpen", ErrorMsg, r, _FL);
			}
			else
			{
				ZeroObj(d->InHdr);
				d->InHdr.lpData = d->InBuf;
				d->InHdr.dwBufferLength = sizeof(d->InBuf);

				r = midiInPrepareHeader(d->hIn, &d->InHdr, sizeof(d->InHdr));
				if (r != MMSYSERR_NOERROR)
				{
					OnError("midiInPrepareHeader", ErrorMsg, r, _FL);
				}
				else
				{
					r = midiInAddBuffer(d->hIn, &d->InHdr, sizeof(d->InHdr));
					if (r != MMSYSERR_NOERROR && GetLog())
						GetLog()->Print("%s:%i - midiInAddBuffer failed with %i\n", _FL, r);
				}

				r = midiInStart(d->hIn);
				if (r != MMSYSERR_NOERROR)
				{
					OnError("midiInStart", ErrorMsg, r, _FL);
				}
				else
				{
					Status = true;
					if (GetLog())
						GetLog()->Print("Created midi connections ok.\n");
				}
			}
		}

	#elif defined(LINUX)

		if (InIdx != OutIdx)
		{
			if (ErrorMsg)
				*ErrorMsg = "Different indexes not supported yet.";
		}
		else if (InIdx < 0 || InIdx >= In.Length())
		{
			if (ErrorMsg)
				*ErrorMsg = "Invalid index.";
		}
		else
		{				
			d->Hnd = open(In[InIdx], O_RDWR);
			if (IsMidiOpen())
			{
				Status = true;
				d->Reader.Reset(new ReaderThread(d));
			}
			else if (ErrorMsg)
			{
				int e = errno;
				auto msg = LErrorCodeToString(e);
				ErrorMsg->Printf("Error opening the device: %i (%s)\n", e, msg.Get());
			}
		}

	#endif
	
	return Status;
}

void LMidi::CloseMidi()
{
	#if defined(MAC)

		if (d->Client)
		{
			MIDIClientDispose(d->Client);
			d->Client = 0;
		}

	#elif defined(WINDOWS)

		if (d->hIn)
		{
			midiInReset(d->hIn);
			midiInUnprepareHeader(d->hIn, &d->InHdr, sizeof(d->InHdr));
			midiInClose(d->hIn);
			d->hIn = 0;
		}
		if (d->hOut)
		{
			midiOutReset(d->hOut);
			midiOutClose(d->hOut);
			d->hOut = 0;
		}

	#elif defined(LINUX)

		if (d->Hnd >= 0)
		{
			close(d->Hnd);
			d->Hnd = -1;
		}

	#endif
}

void LMidi::OnMidiIn(uint8_t *midi, size_t midi_len)
{
	#if MIDI_MIRROR_IN_TO_OUT
	SendMidi(p, len);
	#endif
}

void LMidi::SendMidi(uint8_t *ptr, size_t len, bool quiet)
{
	LAssert(ptr != NULL);
	if (!ptr)
		return;

	#if defined(WINDOWS)
	
		if (d->hOut)
		{
			if (len <= 3)
			{
				DWORD dwMsg = 0;
				uint8_t *b = (uint8_t*) &dwMsg;
				memcpy(b, ptr, len);
				MMRESULT r = midiOutShortMsg(d->hOut, dwMsg);
				if (r != 0)
				{
					if (GetLog())
						GetLog()->Print("midiOutShortMsg failed with %i\n", r);
				}

				if (!quiet)
					OnMidiOut(ptr, len);
			}
			else
			{
				MIDIHDR Hdr;
				ZeroObj(Hdr);
				Hdr.dwUser = quiet;
				Hdr.lpData = (LPSTR) ptr;
				Hdr.dwBufferLength = (DWORD)len;
				midiOutPrepareHeader(d->hOut, &Hdr, sizeof(Hdr));

				MMRESULT r = midiOutLongMsg(d->hOut, &Hdr, sizeof(Hdr));
				if (r != MMSYSERR_NOERROR)
				{
					if (GetLog())
						GetLog()->Print("midiOutLongMsg failed with %i\n", r);
				}

				midiOutUnprepareHeader(d->hOut, &Hdr, sizeof(Hdr));
			}
		}

	#elif defined MAC

		if (d->OutPort)
		{
			struct MIDIPacketList p;
			p.numPackets = 1;
			p.packet[0].timeStamp = 0;
			p.packet[0].length = len;
			memcpy(p.packet[0].data, ptr, len);
			OSStatus e = MIDISend(d->OutPort, d->Dst, &p);
			if (e)
			{
				if (GetLog())
					GetLog()->Print("%s:%i - MIDISend failed %e\n", _FL, e);
			}
			else if (!quiet)
			{
				if (GetLog())
					OnMidiOut(ptr, len);
			}
		}

	#elif defined LINUX

		if (IsMidiOpen())
		{
			auto wr = write(d->Hnd, ptr, len);
			if (wr < len)
				printf("%s:%i - Warning: failed to write all bytes " LPrintfSizeT " " LPrintfSizeT "\n", _FL, len, wr);
			if (wr > 0 && !quiet)
				OnMidiOut(ptr, wr);
		}

	#endif
}

void LMidi::OnMidiOut(uint8_t *p, size_t len)
{
	if (GetLog())
	{
		GetLog()->Print("Out: ");
		for (int i=0; i<len; i++)
			GetLog()->Print("%02.2x ", p[i]);
		GetLog()->Print("\n");
	}

	#if MIDI_MIRROR_IN_TO_OUT
	SendMidi(p, len);
	#endif
}

void LMidi::OnError(char *Func, LString *ErrorMsg, uint32_t Code, char *File, int Line)
{
	char m[256];
	sprintf_s(m, sizeof(m), "%s failed with %i", Func, Code);
	if (ErrorMsg)
		*ErrorMsg = m;
	if (GetLog())
		GetLog()->Print("%s:%i - %s\n", File, Line, m);
}
