#include <Windows.h>
#include <Fltuser.h>

#include <iostream>
#include <string>

static const LPCWSTR COMPORT_NAME = L"\\EvtDrvPort";

int wmain(int argc, wchar_t* argv[])
{
    HANDLE port = NULL;
    HRESULT result = FilterConnectCommunicationPort(
		COMPORT_NAME,                   // PortName
		FLT_PORT_FLAG_SYNC_HANDLE,      // Options
		NULL,                           // Context
		0,                              // ContextSize
		NULL,                           // SecurityAttributes
		&port);                         // ServerPort

    if (result != S_OK)
    {
        std::cerr << "FilterConnectCommunicationPort failed -> HRESULT: " << std::hex << result << std::endl;
        return 1;
    }

	std::cout << "Connected to filter port" << std::endl;

    //byte buffer[256];
    //result = FilterGetMessage(
    //    port,                           // Port
    //    reinterpret_cast<PFILTER_MESSAGE_HEADER>(buffer),// Buffer
    //    sizeof(buffer),                 // BufferSize
    //    NULL);                          // BytesRead
    //std::cout << "FilterGetMessage result: " << std::hex << result << std::endl;

    //Sleep(50000);

    CloseHandle(port);

    return 0;
}

