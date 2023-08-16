#define _CRT_SECURE_NO_WARNINGS

#include "Control.h"

// Built-in Libraries
#include <Windows.h>
#include <Fltuser.h>

// Shared Libraries
#include <Shared.h>
#include <Win32.h>
#include <Communication.h>


class FilterConnection
{
private:
    HANDLE _port = NULL;
public:
    FilterConnection(const std::wstring& comport_name, const std::string& comport_password)
    {
        Communication::Credential credential;
        if (comport_password.length() > sizeof(credential.Password) - 1)
            throw std::runtime_error("password invalid");

        strcpy(reinterpret_cast<char*>(credential.Password), comport_password.c_str());
        auto ret = FilterConnectCommunicationPort(
            comport_name.c_str(),
            FLT_PORT_FLAG_SYNC_HANDLE,
            &credential,
            sizeof(credential),
            NULL,
            &_port);
        if (ret != S_OK)
            throw std::runtime_error(shared::format("connect failed -> HRESULT: %X", ret));
    }
    ~FilterConnection() { CloseHandle(_port); }

    template<typename T>
    void Send(const Communication::ClientMessage<T>& message)
    {
        DWORD bytes_written = 0;
        auto ret = FilterSendMessage(_port, (PVOID)&message, message.Header.Size, NULL, 0, &bytes_written);
        Log("Send -> HRESULT: %X", ret);
        if (ret != S_OK)
            throw std::runtime_error(shared::format("FilterSendMessage failed -> HRESULT: %X", ret));
    }
};


bool Control::Protect(const std::vector<std::wstring>& paths) noexcept
{
    try
    {
        std::string username;
        std::string password;
        auto result = Win32::Credential::GetCredential(username, password);
        if (result != ERROR_SUCCESS)
        {
            LogError("Get credential failed: %d", result);
            return false;
        }

        auto connection = FilterConnection(COMPORT_NAME, password);
        Communication::ClientMessage<Communication::MessageProtect> message;
        for (auto& path : paths)
        {
            wcscpy_s(message.Body.DosPath, path.c_str());
            connection.Send(message);
            Log("Protected %ws", path.c_str());
        }
        return true;
    }
    catch (std::exception& e)
    {
        LogError("Got exception: %s", e.what());
        return false;
    }
}

bool Control::Unprotect(const std::vector<std::wstring>& paths) noexcept
{
    try
    {
        std::string username;
        std::string password;
        auto result = Win32::Credential::GetCredential(username, password);
        if (result != ERROR_SUCCESS)
        {
            LogError("Get credential failed: %d", result);
            return false;
        }

        auto connection = FilterConnection(COMPORT_NAME, password);
        Communication::ClientMessage<Communication::MessageUnprotect> message;
        for (auto& path : paths)
        {
            wcscpy_s(message.Body.DosPath, path.c_str());
            connection.Send(message);
            Log("Unprotected %ws", path.c_str());
        }
        return true;
    }
    catch (std::exception& e)
    {
        LogError("Got exception: %s", e.what());
        return false;
    }
}
