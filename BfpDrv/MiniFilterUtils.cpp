#include <fltKernel.h>
#include "MiniFilterUtils.h"

#include <Shared.h>

_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS MiniFilterUtils::GetFileId(
    _In_ PFLT_INSTANCE Instance,
    _In_ PFILE_OBJECT FileObject,
    _Out_ FILE_ID& FileId)
{
    FileId = {};
    FLT_FILESYSTEM_TYPE type;
    auto status = FltGetFileSystemType(Instance, &type);
    if (status != STATUS_SUCCESS)
        return status;
    if (type == FLT_FSTYPE_REFS)
    {
        FILE_ID_INFORMATION fileIdInformation;
        status = FltQueryInformationFile(
            Instance,                           // Instance
            FileObject,                         // FileObject
            &fileIdInformation,                 // FileInformation
            sizeof(FILE_ID_INFORMATION),        // Length
            FileIdInformation,                  // FileInformationClass
            NULL);                              // ReturnLength
        if (status == STATUS_SUCCESS)
            RtlCopyMemory(&FileId.FileId128, &fileIdInformation.FileId, sizeof(FileId.FileId128));
    }
    else
    {
        FILE_INTERNAL_INFORMATION fileInternalInformation;
        status = FltQueryInformationFile(
            Instance,                           // Instance
            FileObject,                         // FileObject
            &fileInternalInformation,           // FileInformation
            sizeof(FILE_INTERNAL_INFORMATION),  // Length
            FileInternalInformation,            // FileInformationClass
            NULL);                              // ReturnLength
        if (status == STATUS_SUCCESS)
        {
            FileId.FileId64.Value = fileInternalInformation.IndexNumber.QuadPart;
            FileId.FileId64.Reserved = 0LL;
        }
    }
    return status;
}