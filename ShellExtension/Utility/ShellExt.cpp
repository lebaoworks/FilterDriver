#include "ShellExt.h"

// Builtin Libraries
#include <windows.h>

// Shared Libraries
#include <Shared.h>


LONG _DllRefCount = 0;
LONG ShellExt::DllAddRef(void) { Log(); return InterlockedIncrement(&_DllRefCount); }
LONG ShellExt::DllRelease(void) { Log(); return InterlockedDecrement(&_DllRefCount); }


std::string ShellExt::GuidToString(const GUID& guid) { return shared::format("{%08X-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}", guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]); }