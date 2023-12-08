#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define INIT_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID DECLSPEC_SELECTANY name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

INIT_GUID(CLSID_ShellMenu, 0xb0d35103, 0x86a1, 0x471c, 0xa6, 0x53, 0xe1, 0x30, 0xe3, 0x43, 0x9a, 0x3c);

#define SHELL_EXTENSION_CLASS_NAME "BfpShell.ShellExt"
#define SHELL_EXTENSION_DESCRIPTION "ShellExt Class"