#include "../src/GitRefNames.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

#define CHECK(condition) \
	do { if (!(condition)) { \
		std::fprintf(stderr, "Line %d: %s\n", __LINE__, #condition); \
		return false; \
	} } while (false)

static std::vector<std::string> NameList(LString names)
{
	std::vector<std::string> result;
	for (auto name: names.Split(","))
		result.emplace_back(name.Get());
	std::sort(result.begin(), result.end());
	return result;
}

static bool TestLookups()
{
	GitRefNames names;
	names.Add("abcdef0", "origin/main");
	names.Add("abcdef0", "main");
	names.Add("abcdef0123", "release");
	names.Add("abcdef0123456789abcdef0123456789abcdef01", "full-hash");
	names.Add("1234567890abcdef", "other");

	CHECK(NameList(names.Find("ABCDEF0123456789ABCDEF0123456789ABCDEF01")) ==
		NameList("origin/main,main,release,full-hash"));
	CHECK(NameList(names.Find("abcdef0")) == NameList("origin/main,main"));
	CHECK(!names.Find("abcdef"));
	CHECK(!names.Find("ffffff0123456789"));
	CHECK(!names.Find(LString()));

	names.Add("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
		"sha256");
	CHECK(names.Find("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef") ==
		"sha256");
	return true;
}

static bool TestCacheInvalidation()
{
	GitRefNames names;
	CHECK(!names.Find("abcdef0123456789"));
	CHECK(!names.Find("abcdef0123456789"));
	names.Add("abcdef0", "main");
	CHECK(names.Find("abcdef0123456789") == "main");
	names.Add("abcdef0", "origin/main");
	CHECK(NameList(names.Find("abcdef0123456789")) == NameList("main,origin/main"));

	names.Empty();
	CHECK(!names.Find("abcdef0123456789"));
	names.Add("1234567", "main");
	CHECK(!names.Find("abcdef0123456789"));
	CHECK(names.Find("1234567890abcdef") == "main");
	return true;
}

static bool Benchmark(const char *refsPath, const char *commitsPath)
{
	std::ifstream refs(refsPath), commits(commitsPath);
	CHECK(refs.is_open() && commits.is_open());
	GitRefNames names;
	LHashTbl<ConstStrKey<char>, LString> oldNames;
	std::string hash, name;
	while (refs >> hash >> name)
	{
		names.Add(hash.c_str(), name.c_str());
		auto existing = oldNames.Find(hash.c_str());
		oldNames.Add(hash.c_str(), existing ? existing + "," + name.c_str() : LString(name.c_str()));
	}
	CHECK(refs.eof() && oldNames.Length() > 0);

	LArray<LString> hashes, expected, actual;
	while (commits >> hash)
		hashes.Add(hash.c_str());
	CHECK(commits.eof() && hashes.Length() > 0);

	auto started = std::chrono::steady_clock::now();
	for (auto h: hashes)
	{
		LString result;
		for (auto ref: oldNames)
		{
			if (!Strnicmp(h.Get(), ref.key, strlen(ref.key)))
			{
				if (result)
					result += ",";
				result += ref.value;
			}
		}
		expected.Add(result);
	}
	auto scanned = std::chrono::steady_clock::now();
	for (auto h: hashes)
		actual.Add(names.Find(h));
	auto indexed = std::chrono::steady_clock::now();
	for (size_t i = 0; i < hashes.Length(); i++)
		CHECK(NameList(expected[i]) == NameList(actual[i]));

	auto warmStarted = std::chrono::steady_clock::now();
	size_t bytes = 0;
	for (auto h: hashes)
		bytes += names.Find(h).Length();
	auto done = std::chrono::steady_clock::now();
	auto ms = [](auto duration)
	{
		return std::chrono::duration<double, std::milli>(duration).count();
	};
	std::printf("refs=%zu commits=%zu scan_ms=%.3f indexed_cold_ms=%.3f cached_ms=%.3f name_bytes=%zu\n",
		oldNames.Length(), (size_t)hashes.Length(), ms(scanned - started),
		ms(indexed - scanned), ms(done - warmStarted), bytes);
	return true;
}

int main(int argc, char **argv)
{
	if (!TestLookups() || !TestCacheInvalidation())
		return 1;
	std::puts("Git ref lookup tests passed.");
	if (argc == 3)
		return Benchmark(argv[1], argv[2]) ? 0 : 1;
	if (argc != 1)
	{
		std::fprintf(stderr, "Usage: %s [refs-file commits-file]\n", argv[0]);
		return 1;
	}
	return 0;
}
