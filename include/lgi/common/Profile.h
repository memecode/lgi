//
//  Profile.h
//  LgiCocoa
//
//  Created by Matthew Allen on 2/12/21.
//  Copyright © 2021 Memecode. All rights reserved.
//
#pragma once

/// This class makes it easy to profile a function and write out timings at the end
class LgiClass LProfile
{
	struct Sample
	{
		uint64 Time;
		const char *Name;
		
		Sample(uint64 t = 0, const char *n = 0)
		{
			Time = t;
			Name = n;
		}
	};
	
	LArray<Sample> s;
	char *Buf;
	int Used;
	int MinMs;
	
public:
	LProfile(const char *Name, int HideMs = -1);
	virtual ~LProfile();
	
	void HideResultsIfBelow(int Ms);
	virtual void Add(const char *Name);
	virtual void Add(const char *File, int Line);
};

// This code will assert if the cast fails.
template<typename A, typename B>
A &AssertCast(A &a, B b)
{
	a = (A) b;	 // If this breaks it'll assert.
	LAssert((B)a == b);
	return a;
}

