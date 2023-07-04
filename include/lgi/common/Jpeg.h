#if !defined(_GJPEG_H_) && HAS_LIBJPEG
#define _GJPEG_H_

class GdcJpeg : public LFilter
{
public:
    enum SubSampleMode
    {
        Sample_1x1_1x1_1x1,
        Sample_2x2_1x1_1x1,
        Sample_2x1_1x1_1x1,
        Sample_1x2_1x1_1x1,
    };

private:
	friend class LJpegOptions;
	#if LIBJPEG_SHARED
	class LibJpeg *d;
	#endif

	LFilter::IoStatus _Write(LStream *Out, LSurface *pDC, int Quality, SubSampleMode SubSample, LPoint Dpi);

public:
	GdcJpeg();
	
	const char *GetClass() override { return "GdcJpeg"; }
    const char *GetComponentName() { return "libjpeg"; }
	Format GetFormat() { return FmtJpeg; }
	int GetCapabilites() { return FILTER_CAP_READ | FILTER_CAP_WRITE; }
	IoStatus ReadImage(LSurface *pDC, LStream *In);
	IoStatus WriteImage(LStream *Out, LSurface *pDC);

	bool GetVariant(const char *n, LVariant &v, const char *a)
	{
		if (!_stricmp(n, LGI_FILTER_TYPE))
		{
			v = "Jpeg";
		}
		else if (!_stricmp(n, LGI_FILTER_EXTENSIONS))
		{
			v = "JPG,JPEG";
		}
		else return false;

		return true;
	}
};

#endif