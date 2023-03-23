/// \file
/// \author Matthew Allen <fret@memecode.com>
#pragma once

#ifdef __cplusplus

#include "lgi/common/HashTable.h"

#define DEBUG_CAPABILITIES		0

class LCapabilityTarget;

/// This class is a parent for objects that need external dependencies 
/// to be installed by an external source.
class LgiClass LCapabilityClient
{
	friend class LCapabilityTarget;
	LArray<LCapabilityTarget*> Targets;

public:
	virtual ~LCapabilityClient();
	
	/// Call this when you need to fulfill an external dependency.
	bool NeedsCapability(const char *Name, const char *Param = NULL);

	/// Call this to register a target that can install dependencies.
	void Register(LCapabilityTarget *t);
};

class LgiClass LCapabilityTarget
{
	friend class LCapabilityClient;
	LArray<LCapabilityClient*> Clients;

public:
	LHashTbl<ConstStrKey<char,false>, bool> Map;
	typedef LHashTbl<ConstStrKey<char,false>, bool> CapsHash;

	virtual ~LCapabilityTarget();
	
	/// This is called to install a dependency.
	virtual bool NeedsCapability(const char *Name, const char *Param) = 0;
	
	/// This is called after the dependancy is installed.
	virtual void OnInstall(CapsHash *Caps, bool Status) = 0;

	/// This is called if the user closes the capability bar...
	virtual void OnCloseInstaller() = 0;
};

#endif
