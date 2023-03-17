/* Levenshtein.h - levenshtein algorithm.
 * MIT licensed.
 * Copyright (c) 2015 Titus Wormer <tituswormer@gmail.com>
 *
 * Returns an unsigned integer, depicting
 * the difference between `a` and `b`.
 * See http://en.wikipedia.org/wiki/Levenshtein_distance
 * for more information.
 */

#pragma once

inline
size_t
LLevenshtein(const char *a, const char *b)
{
	size_t aLen = Strlen(a);
	auto cache = (size_t*)calloc(aLen, sizeof(size_t));
	size_t result = 10000;

	/* Shortcut optimizations / degenerate cases. */
	if (a == b || cache == NULL)
		return 0;

	size_t bLen = Strlen(b);
	if (aLen == 0)
	{
		result = bLen;
		goto onComplete;
	}
	if (bLen == 0)
	{
		result = aLen;
		goto onComplete;
	}

	size_t aIndex = 0;
	size_t bIndex = 0;
	size_t distance;
	
	/* initialize the vector. */
	while (aIndex < aLen)
	{
		cache[aIndex] = aIndex + 1;
		aIndex++;
	}

	/* Loop. */
	while (bIndex < bLen)
	{
		auto code = b[bIndex];
		result = distance = bIndex++;
		aIndex = -1;

		while (++aIndex < aLen)
		{
			auto bDistance = code == a[aIndex] ? distance : distance + 1;
			distance = cache[aIndex];

			cache[aIndex] = result = distance > result
				? bDistance > result
					? result + 1
					: bDistance
				: bDistance > distance
					? distance + 1
					: bDistance;
		}
	}

onComplete:
	free(cache);
	return result;
}
