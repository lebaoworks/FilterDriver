#pragma once

#include <ntdef.h>
#include <ntstatus.h>
template <typename T>
struct failable_object
{
protected: T _error;
public:    T error() const noexcept = 0;
};

template<>
struct failable_object<bool>
{
protected: bool _error = false;
public:    bool error() const noexcept { return _error; };
};

template<>
struct failable_object<NTSTATUS>
{
protected: NTSTATUS _error = STATUS_SUCCESS;
public:    NTSTATUS error() const noexcept { return _error; };
};