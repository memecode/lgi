// This class wraps ffmpeg via FFMS to load video frames from a file.
// https://github.com/FFMS/ffms2
#pragma once

#include "ffms.h"

class FfmpegFile
{
protected:
	FFMS_VideoSource *videosource = NULL;
	FFMS_Indexer *indexer = NULL;
	FFMS_Index *index = NULL;
	FFMS_Track *track = NULL;
	FFMS_ErrorInfo errinfo;
	const FFMS_VideoProperties *videoprops = NULL;

	char errmsg[1024] = "";
	int Fmt = 0;
	int videoTrack = -1;

	LStream	NullLog;

public:
	int numFrames = 0;
	LStream *Log = NULL;
	LPoint Size; // Frame size

	FfmpegFile(const char *FileName = NULL) : Log(&NullLog)
	{
		LAssert(Log);

		FFMS_Init(0, 0);
		errinfo.Buffer      = errmsg;
		errinfo.BufferSize  = sizeof(errmsg);
		errinfo.ErrorType   = FFMS_ERROR_SUCCESS;
		errinfo.SubType     = FFMS_ERROR_SUCCESS;

		if (FileName)
			Open(FileName);
	}

	~FfmpegFile()
	{
		if (indexer)
		{
			FFMS_CancelIndexing(indexer);
			indexer = NULL;
		}
		else if (index)
		{
			FFMS_DestroyIndex(index);
			index = NULL;
		}
	
		index = NULL;
		videosource = NULL;
		track = NULL;
		videoprops = NULL;
	}

	bool Open(const char *InputFile)
	{
		indexer = FFMS_CreateIndexer(InputFile, &errinfo);
		if (indexer == NULL)
		{
			Log->Print("%s:%i - FFMS_CreateIndexer(%s) failed.\n", _FL, InputFile);
			return false;
		}
		else Log->Print("Open(%s) ok.\n", InputFile);

		index = FFMS_DoIndexing2(indexer, FFMS_IEH_ABORT, &errinfo);
		indexer = NULL; // FFMS_DoIndexing2 will delete the indexer...
		if (index == NULL)
		{
			Log->Print("%s:%i - FFMS_DoIndexing2 failed.\n", _FL);
			return false;
		}
		else Log->Print("Index ok.\n");

		videoTrack = FFMS_GetFirstTrackOfType(index, FFMS_TYPE_VIDEO, &errinfo);
		Log->Print("videoTrack = %i.\n", videoTrack);

		videosource = FFMS_CreateVideoSource(InputFile, videoTrack, index, 1, FFMS_SEEK_NORMAL, &errinfo);
		if (videosource == NULL)
		{
			Log->Print("%s:%i - FFMS_CreateVideoSource failed.\n", _FL);
			return false;
		}
		else Log->Print("Video source ok.\n");

		videoprops = FFMS_GetVideoProperties(videosource);
		if (videoprops)
		{
			numFrames = videoprops->NumFrames;
		}

		track = FFMS_GetTrackFromIndex(index, videoTrack);
		if (track == NULL)
		{
			Log->Print("%s:%i - FFMS_GetTrackFromIndex failed.\n", _FL);
			return false;
		}
		else Log->Print("Video track ok.\n");

		return true;
	}
	
	LAutoPtr<LSurface> LoadFrame(int FrameIdx)
	{
		LAutoPtr<LSurface> Frame;

		if (Size.x == 0)
		{
			const FFMS_Frame *propframe = FFMS_GetFrame(videosource, FrameIdx, &errinfo);
			if (!propframe)
			{
				Log->Print("Error: FFMS_GetFrame failed @ %s:%i.\n", _FL);
				return Frame;
			}

			if (videoprops->SARNum && videoprops->SARDen)
				Size.x = videoprops->SARNum * propframe->EncodedWidth / videoprops->SARDen; // frame width in pixels
			else
				Size.x = propframe->EncodedWidth;
			Size.y = propframe->EncodedHeight; // frame height in pixels
			Fmt = propframe->EncodedPixelFormat; // actual frame colorspace
		}

		/* If you want to change the output colorspace or resize the output frame size,
		now is the time to do it. IMPORTANT: This step is also required to prevent
		resolution and colorspace changes midstream. You can you can always tell a frame's
		original properties by examining the Encoded* properties in FFMS_Frame.
		
		See libavutil/pixfmt.h for the list of pixel formats/colorspaces.
		To get the name of a given pixel format, strip the leading PIX_FMT_
		and convert to lowercase. For example, PIX_FMT_YUV420P becomes "yuv420p". */

		/* A -1 terminated list of the acceptable output formats. */
		int pixfmts[2];
		#ifdef WINDOWS
			pixfmts[0] = FFMS_GetPixFmt("bgra");
		#else
			pixfmts[0] = FFMS_GetPixFmt("rgba");
		#endif
		pixfmts[1] = -1;

		double scale = 1.0;
		int Sx = (int)(Size.x * scale), Sy = (int)(Size.y * scale);
		if (FFMS_SetOutputFormatV2(videosource, pixfmts, Sx, Sy, FFMS_RESIZER_FAST_BILINEAR/*FFMS_RESIZER_BICUBIC*/, &errinfo))
		{
			Log->Print("Error: FFMS_SetOutputFormatV2 failed @ %s:%i, frame: %i x %i, size: %i x %i, scale=%.2f\n",
				_FL,
				Size.x, Size.y,
				Sx, Sy,
				scale);
			return Frame;
		}

		/* now we're ready to actually retrieve the video frames */
		const FFMS_Frame *curframe = FFMS_GetFrame(videosource, FrameIdx, &errinfo);
		if (curframe == NULL)
		{
			Log->Print("Error: FFMS_GetFrame failed @ %s:%i.\n", _FL);
			return Frame;
		}

		if (Frame.Reset(new LMemDC(_FL, Sx, Sy, System32BitColourSpace)))
		{	
			for (int y=0; y<Sy; y++)
			{
				auto in = (System32BitPixel*) (curframe->Data[0] + (y * curframe->Linesize[0]));
				auto out = (System32BitPixel*) (*Frame)[y];
				if (!out)
				{
					LAssert(!"No scanline");
					break;
				}

				memcpy(out, in, MIN(Frame->GetRowStep(), curframe->Linesize[0]));
			}
		}

		return Frame;
	}
	
	
};
