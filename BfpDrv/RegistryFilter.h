#pragma once

#include <fltKernel.h>
#include <Shared.h>

class RegistryFilter : public failable_object<NTSTATUS>
{
private:
    PFLT_FILTER _filter = NULL;
    UNICODE_STRING* _registry_path;

public:
    RegistryFilter(_In_ UNICODE_STRING* RegistryPath);
    ~RegistryFilter();
};

