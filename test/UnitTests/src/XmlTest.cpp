#include "lgi/common/Lgi.h"
#include "lgi/common/XmlTree.h"
#include "UnitTests.h"

class XmlTestPriv
{
};

XmlTest::XmlTest() : UnitTest("LXmlTest")
{
}

XmlTest::~XmlTest()
{
}

bool XmlTest::Run()
{
	{
		LXmlTree tree;
		LXmlTag root;
		LFile f;
		LFile::Path path(LSP_APP_INSTALL);

		path = path / "data" / "Folder.xml";
		if (!f.Open(path, O_READ))
			return FAIL(_FL, "Can't open text xml file.");

		if (!tree.Read(&root, &f))
			return FAIL(_FL, "Xml read failed");
	}

	#ifdef LGI_UNIT_TESTS
	if (LXmlTag::Instances > 0)
		return FAIL(_FL, "Not all tags were deleted.");
	#endif

	return true;
}

