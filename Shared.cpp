#include "Shared.h"

#include <wdm.h>
void shared::log(_In_ unsigned int level, _In_z_ char* szString)
{
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, level, "%s", szString);
}
