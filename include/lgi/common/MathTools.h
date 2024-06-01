#pragma once


template<typename T>
struct LKernel : public LArray<T>
{
	int size = 0;
	T *center = NULL;
	double total = 1.0;
	
	LKernel(int sz, double sigma) : size(sz)
	{
		this->SetFixedLength(false);
		this->Length(size * 2 + 1);
		center = &(*this)[size];

		// gaussian_filter = [1 / (sigma * np.sqrt(2*np.pi)) * np.exp(-x**2/(2*sigma**2))
		for (int i=0; i<size; i++)
		{
			T val = 1.0
					/
					(
						sigma
						*
						sqrt(2 * LGI_PI)
						*
						exp
						(
							pow(-i, 2)
							/
							(
							   2 * pow(sigma, 2)
							)
						 )
					 );
			center[i] = val;
			if (i)
				center[-i] = val;
		}
	}
	
	void Normalize()
	{
		total = 0;
		
		auto c = *center;
		for (unsigned i=0; i<this->Length(); i++)
		{
			(*this)[i] = (*this)[i] / c;
			total += (*this)[i];
		}
	}
};

template<typename T>
LArray<T> LSmooth(LArray<T> &a, double amount)
{
	LArray<T> out;
	auto k = LKernel<double>((int)amount, amount);
	
	k.Normalize();
	#if 0
	for (unsigned i = 0; i < k.Length(); i++)
		printf("%u = %f\n", i, k[i]);
	#endif
	
	out.Length(a.Length());
	for (unsigned i = 0; i < a.Length(); i++)
	{
		double sum = 0.0;
		for (int n = -k.size; n < k.size; n++)
		{
			int idx = (int)i + n;
			sum += (a.IdxCheck(idx) ? (double)a[idx] : 0.0) * k.center[n];
		}
		out[i] = (T) (sum / k.total);
	}
	
	#if 0
	for (unsigned i = 0; i < out.Length(); i++)
		printf("%u = %i\n", i, out[i]);
	#endif

	return out;
}

template<typename T>
LAutoPtr<LSurface> LGraph(LArray<T> &data, LColour col = LColour::Blue)
{
    LAutoPtr<LSurface> img(new LMemDC((int)data.Length(), 256, System32BitColourSpace));
    img->Colour(LColour::White);
    img->Rectangle();
    img->Colour(col);

    T max = 0;
    for (auto d: data)
        max = MAX(max, d);

    LPoint prev;
    for (int x=0; x<data.Length(); x++)
    {
        LPoint pt(x, 255 - (data[x] * 255 / max));
        if (x)
            img->Line(prev.x, prev.y, pt.x, pt.y);
        prev = pt;
    }

    return img;
}

template<typename T>
T LMax(LArray<T> &a)
{
	T max = 0;
	for (auto i: a)
		if (i > max)
			max = i;
	return max;
}

template<typename T>
T LMin(LArray<T> &a)
{
	if (a.Length() == 0)
		return 0;
	T min = a[0];
	for (size_t i=1; i<a.Length(); i++)
		if (a[i] < min)
			min = a[i];
	return min;
}

template<typename T>
T LMedian(LArray<T> &a)
{
	a.Sort();
	return a[a.Length()>>1];
}

template<typename T>
T LAverage(LArray<T> &a)
{
	if (a.Length() == 0)
		return 0;

	T sum = 0;
	for (auto i: a)
		sum += i;

	return (T) (sum / a.Length());
}

template<typename T, typename S>
void LDetectPeaks(
        LArray<T>	&input,
        LArray<S>	&peaks,          // peaks will be put here
        double		delta)           // delta used for distinguishing peaks
{
	int prevIdx = -1;
	for (T i=0; i<input.Length(); i++)
	{
		T in = input[i];
		bool isMax = true;

		for (T n=-delta; isMax && n<delta; n++)
		{
			if (n == 0) continue;
			if (input.IdxCheck(i+n))
			{
				T val = input[i+n];
				if (val > in)
					isMax = false;
			}
		}
		if (isMax && (prevIdx < 0 || i - prevIdx > delta))
			peaks.Add(prevIdx = i);
	}
}
