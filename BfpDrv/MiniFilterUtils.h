#pragma once
#include <fltKernel.h>

namespace MiniFilterUtils
{
    union FILE_ID
    {
        struct
        {
            UINT64 Value;
            UINT64 Reserved;
        } FileId64;
        FILE_ID_128 FileId128;
    };

    // Get the file ID for the given file object. Does not work in PRE_CREATE callback.
    _IRQL_requires_(PASSIVE_LEVEL) NTSTATUS GetFileId(_In_ PFLT_INSTANCE Instance, _In_ PFILE_OBJECT FileObject, _Out_ FILE_ID& FileId);
};