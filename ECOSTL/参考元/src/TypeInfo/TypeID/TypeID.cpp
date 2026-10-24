#include "TypeID.h"

namespace utl
{
	void TypeIDAllocator::Allocate(const char* a_keyName)
	{
		TypeID nID = ++counter_;
		umNameToTypeID_[a_keyName] = nID;
	}
}