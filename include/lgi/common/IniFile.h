#pragma once

class LIniFile
{
	LSsh *ssh = nullptr;
	LString filePath;
	LString::Array lines;

	LString IsSection(LString &ln)
	{
		if (!ln)
			return LString();
		char *s = ln.Get();
		const char *start = NULL, *end = NULL;
		for (auto i=s; *i; i++)
		{
			if (!start)
			{
				if (*i == '[') start = i;
			}
			else if (!end)
			{
				if (*i == ']') end = i;
			}
		}
		if (!start || !end)
			return LString();
		start++;
		return LString(start, end-start);
	}

	LRange GetSection(LString section)
	{
		LRange rng;
		rng.Start = -1;

		LString curSection, sect = section.Lower();
		for (size_t i=0; i<lines.Length(); i++)
		{
			auto &ln = lines[i];
			if (auto s = IsSection(ln))
			{
				curSection = s.Lower();
				if (curSection == sect)
				{
					// Found the start
					rng.Start = i + 1;
				}
				else if (rng.Start >= 0)
				{
					// Found the end..
					LAssert((ssize_t)i >= rng.Start);
					rng.Len = i - rng.Start;
					break;
				}
			}			
		}

		return rng;
	}

public:
	LIniFile(LSsh *Ssh, const char *Path = NULL)
	{
		ssh = Ssh;
		if (Path)
			Read(Path);
	}

	operator bool() const
	{
		return lines.Length() > 0;
	}

	bool Read(const char *Path)
	{
		if (ssh)
		{
			LStringPipe p;
			auto err = ssh->DownloadFile(&p, Path);
			if (err)
				return false;

			lines = p.NewLStr().Replace("\r").SplitDelimit("\n", -1, false);
		}
		else
		{
			auto txt = LReadFile(filePath = Path);
			if (!txt)
				return false;
			lines = txt.Replace("\r", "").SplitDelimit("\n", -1, false);
		}

		return true;
	}

	bool Write(const char *Path = NULL)
	{
		if (Path)
			filePath = Path;
		if (!filePath)
			return false;

		auto content = LString("\n").Join(lines);
		if (ssh)
		{
			LMemStream wrapper(content.Get(), content.Length(), false);
			auto err = ssh->UploadFile(filePath, &wrapper);
			return !err;
		}
		else
		{
			LFile out(filePath, O_WRITE);
			if (!out)
				return false;
			out.SetSize(0);
			return out.Write(content);
		}
	}

	LString Get(LString section, LString var)
	{
		auto rng = GetSection(section);
		if (!rng)
			return LString();

		for (ssize_t i=rng.Start; i<=rng.End(); i++)
		{
			auto &ln = lines[i];
			auto p = ln.SplitDelimit("=", 1);
			if (p.Length() == 2 &&
				p[0].Strip() == var)
			{
				return p[1].Strip();
			}
		}

		return LString();
	}

	bool Set(LString section, LString var, LString value)
	{
		auto rng = GetSection(section);
		auto newVar = LString::Fmt("%s=%s", var.Get(), value.Get());

		if (rng)
		{
			// Check the variable doesn't already exist...
			for (ssize_t i=rng.Start; i<=rng.End(); i++)
			{
				auto &ln = lines[i];
				auto p = ln.SplitDelimit("=", 1);
				if (p.Length() == 2 &&
					p[0].Strip() == var)
				{
					// It does exist... change the value
					ln = newVar;
					return true;
				}
			}

			// Doesn't already exist... add to the section a new line
			lines.SetFixedLength(false);
			auto add = lines.AddAt(rng.Start, newVar);
			LAssert(add);
			return add;
		}
		else
		{
			// Create the section and add the variable
			lines.SetFixedLength(false);
			lines.New().Printf("[%s]", section.Get());
			lines.New() = newVar;
		}

		return true;
	}
};
