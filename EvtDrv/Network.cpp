/********************
*     Includes      *
********************/

#include "Network.hpp"

#define NDIS_SUPPORT_NDIS620 1      // Enable NDIS 6.20 features (Required for Windows 10 and above, adjust as needed for compatibility)
#include <fwpsk.h>
#include <fwpmk.h>
#ifdef NTDDI_VERSION
#endif

// Logging via tracing
#include "trace.h"
#include "Network.tmh"

/*********************
*    Declarations    *
*********************/

// {335111A6-D3E4-4558-A252-85EA32BCDA2E}
DEFINE_GUID(MY_CONNECT_CALLOUT_V4,
    0x335111a6, 0xd3e4, 0x4558, 0xa2, 0x52, 0x85, 0xea, 0x32, 0xbc, 0xda, 0x2e);

namespace WPF
{
    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_requires_same_
    void NTAPI ClassifyCallback(
        _In_ const FWPS_INCOMING_VALUES0* inFixedValues,
        _In_ const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
        _Inout_opt_ void* layerData,
        _In_ const FWPS_FILTER0* filter,
        _In_ UINT64 flowContext,
        _Inout_ FWPS_CLASSIFY_OUT0* classifyOut);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_requires_same_
    NTSTATUS NTAPI NotifyCallback(
        _In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
        _In_ const GUID* filterKey,
        _Inout_ FWPS_FILTER0* filter);

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_requires_same_
    void NTAPI FlowDeleteCallback(
        _In_ UINT16 layerId,
        _In_ UINT32 calloutId,
        _In_ UINT64 flowContext);
}

/*********************
*     Global Vars    *
*********************/
#pragma data_seg("NONPAGED")
static HANDLE EngineHandle = NULL;
static UINT32 CalloutId = 0;
static UINT64 FilterId = 0;

static const FWPS_CALLOUT0 SCallOut = {
    .calloutKey = MY_CONNECT_CALLOUT_V4,
    .classifyFn = WPF::ClassifyCallback,
    .notifyFn = WPF::NotifyCallback,
    .flowDeleteFn = WPF::FlowDeleteCallback,
};
static const FWPM_CALLOUT0 MCallOut = {
    .calloutKey = MY_CONNECT_CALLOUT_V4,
    .displayData = {
        .name = const_cast<LPWSTR>(L"Evt Network Monitor Callout"),
    },
    .applicableLayer = FWPM_LAYER_ALE_AUTH_CONNECT_V4,
};
static const FWPM_FILTER0 MFilter = {
    .displayData = {
        .name = const_cast<LPWSTR>(L"Evt Network Monitor Filter"),
    },
    .layerKey = FWPM_LAYER_ALE_AUTH_CONNECT_V4,
    .action = {
        .type = FWP_ACTION_CALLOUT_UNKNOWN,
        .calloutKey = MY_CONNECT_CALLOUT_V4,
    },
};

static Event::EventNotifyCallback GlobalEventCallback = nullptr;
#pragma data_seg()

namespace WPF
{
    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_requires_same_
    void NTAPI ClassifyCallback(
        _In_ const FWPS_INCOMING_VALUES0* inFixedValues,
        _In_ const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
        _Inout_opt_ void* layerData,
        _In_ const FWPS_FILTER0* filter,
        _In_ UINT64 flowContext,
        _Inout_ FWPS_CLASSIFY_OUT0* classifyOut)
    {
        UNREFERENCED_PARAMETER(inFixedValues);
        UNREFERENCED_PARAMETER(inMetaValues);
        UNREFERENCED_PARAMETER(layerData);
        UNREFERENCED_PARAMETER(filter);
        UNREFERENCED_PARAMETER(flowContext);
        UNREFERENCED_PARAMETER(classifyOut);

        // Remote IP (IPv4)
        UINT32 remoteIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_ADDRESS].value.uint32;
        // Remote Port
        UINT16 remotePort = inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_PORT].value.uint16;

        // Process ID from Metadata
        UINT64 pid = 0;
        if (FWPS_IS_METADATA_FIELD_PRESENT(inMetaValues, FWPS_METADATA_FIELD_PROCESS_ID)) {
            pid = inMetaValues->processId;
        }

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Connection attempt: PID=%6llu -> Remote: %u.%u.%u.%u:%hu",
            pid,
            (remoteIp >> 24) & 0xFF, (remoteIp >> 16) & 0xFF,
            (remoteIp >> 8) & 0xFF, remoteIp & 0xFF,
            remotePort);

        // Allow connection
        classifyOut->actionType = FWP_ACTION_PERMIT;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_requires_same_
    NTSTATUS NTAPI NotifyCallback(
        _In_ FWPS_CALLOUT_NOTIFY_TYPE notifyType,
        _In_ const GUID* filterKey,
        _Inout_ FWPS_FILTER0* filter)
    {
        UNREFERENCED_PARAMETER(notifyType);
        UNREFERENCED_PARAMETER(filterKey);
        UNREFERENCED_PARAMETER(filter);

        return STATUS_SUCCESS;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    _IRQL_requires_same_
    void NTAPI FlowDeleteCallback(
        _In_ UINT16 layerId,
        _In_ UINT32 calloutId,
        _In_ UINT64 flowContext)
    {
        UNREFERENCED_PARAMETER(layerId);
        UNREFERENCED_PARAMETER(calloutId);
        UNREFERENCED_PARAMETER(flowContext);
    }
}

namespace WPF
{
    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Monitor::Monitor(
        _In_ DRIVER_OBJECT* DriverObject,
        _In_ Event::EventNotifyCallback Callback
    ) noexcept
    {
        auto& status = failable::_status;
        GlobalEventCallback = Callback;

        // Open WFP Engine
        status = FwpmEngineOpen0(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &EngineHandle);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Network: Failed to open WFP engine -> status: %!STATUS!", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) FwpmEngineClose0(EngineHandle); };

        status = FwpsCalloutRegister0(DriverObject, &SCallOut, &CalloutId);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Network: Failed to register callout with Fwps -> status: %!STATUS!", status);
            return;
        }
        defer{ if (status != STATUS_SUCCESS) FwpsCalloutUnregisterById0(CalloutId); };

        // Add Callout to Management Engine (Fwpm)
        status = FwpmCalloutAdd0(EngineHandle, &MCallOut, NULL, NULL);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Network: Failed to add callout to management engine -> status: %!STATUS!", status);
            return;
        }

        // Add Filter to activate Callout
        status = FwpmFilterAdd0(EngineHandle, &MFilter, NULL, &FilterId);
        if (status != STATUS_SUCCESS)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Network: Failed to add filter to management engine -> status: %!STATUS!", status);
            return;
        }
    }

    _IRQL_requires_(PASSIVE_LEVEL)
    _IRQL_requires_same_
    Monitor::~Monitor()
    {
        if (this->status() != STATUS_SUCCESS)
            return;

        FwpmFilterDeleteById0(EngineHandle, FilterId);
        FwpmCalloutDeleteById0(EngineHandle, CalloutId);
        FwpsCalloutUnregisterById0(CalloutId);
        FwpmEngineClose0(EngineHandle);
    }
}