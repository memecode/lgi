#include "lgi/common/Lgi.h"
#include "lgi/common/Token.h"

void LShowFileProperties(OsView Parent, const char *Filename)
{
	LAssert(!"Impl me.");
}

bool LBrowseToFile(const char *Filename)
{
	if (!Filename)
		return false;

	if (auto fileBrowser = LGetAppForMimeType("inode/directory"))
	{
	    // fileBrowser may be a single path to an executable or may include arguments 
	    // and possibly a place holder for the path. e.g.:
	    //
	    //      path/to/app[.exe]
	    //      path/to/app[.exe] --arg %U	    
	    LToken tok(fileBrowser);
	    LString::Array out;

    	LString path;
    	if (strchr(Filename, ' '))
    		path.Printf("\"%s\"", Filename);
    	else
    		path = Filename;
	    
	    for (auto &t: tok)
	    {
	        if (t[0] == '%')
	        {
	            // path placeholder...
	            out.Add(path);
	            path.Empty();
	        }
	        else out.Add(t);
	    }
	    if (path)
	        out.Add(path); // didn't see the placeholder... just append it here.	    

        auto exe = out.PopFirst();
        auto args = LString(" ").Join(out);
    	return LExecute(exe, args);
    }
    
    return false;	
}