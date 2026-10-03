#include <Windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>

static void Log(const char* format, ...)
{
    char buffer[512];

    va_list args;
    va_start(args, format);

    vsprintf_s(
        buffer,
        sizeof(buffer),
        format,
        args
    );

    va_end(args);

    OutputDebugStringA(buffer);
}

static uintptr_t FindPattern(
    uintptr_t base,
    size_t size,
    const unsigned char* pattern,
    const char* mask)
{
    size_t patternLength = strlen(mask);

    if (size < patternLength)
        return 0;

    for (size_t i = 0; i <= size - patternLength; i++)
    {
        bool found = true;

        for (size_t j = 0; j < patternLength; j++)
        {
            if (mask[j] != '?' &&
                pattern[j] != *reinterpret_cast<unsigned char*>(
                    base + i + j))
            {
                found = false;
                break;
            }
        }

        if (found)
            return base + i;
    }

    return 0;
}

static uintptr_t GetModuleSize(uintptr_t base)
{
    auto dosHeader =
        reinterpret_cast<PIMAGE_DOS_HEADER>(base);

    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    auto ntHeader =
        reinterpret_cast<PIMAGE_NT_HEADERS64>(
            base + dosHeader->e_lfanew);

    if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
        return 0;

    return ntHeader->OptionalHeader.SizeOfImage;
}

DWORD WINAPI InitializeThread(LPVOID)
{
    Log("[Deadzone] InitializeThread started\n");

    Sleep(5000);

    Log("[Deadzone] 5 seconds elapsed\n");

    uintptr_t base =
        reinterpret_cast<uintptr_t>(
            GetModuleHandleW(nullptr));

    if (!base)
    {
        Log("[Deadzone] GetModuleHandle FAILED\n");
        return 0;
    }

    Log(
        "[Deadzone] Minecraft base = %p\n",
        reinterpret_cast<void*>(base));

    uintptr_t moduleSize = GetModuleSize(base);

    if (!moduleSize)
    {
        Log("[Deadzone] GetModuleSize FAILED\n");
        return 0;
    }

    Log(
        "[Deadzone] Module size = 0x%llX\n",
        static_cast<unsigned long long>(moduleSize));

    const unsigned char pattern[] =
    {
        0xF3, 0x0F, 0x10, 0x35,
        0x00, 0x00, 0x00, 0x00,
         
        0xF3, 0x0F, 0x10, 0x3D,
        0x00, 0x00, 0x00, 0x00,

        0x45, 0x0F, 0x57, 0xDB
    };

    const char mask[] =
        "xxxx????"
        "xxxx????"
        "xxxx";

    Log("[Deadzone] Searching pattern...\n");

    uintptr_t match =
        FindPattern(
            base,
            moduleSize,
            pattern,
            mask);

    if (!match)
    {
        Log("[Deadzone] Pattern NOT FOUND\n");
        return 0;
    }

    Log(
        "[Deadzone] Pattern found at %p\n",
        reinterpret_cast<void*>(match));

    uintptr_t movssAddress = match + 8;

    int32_t displacement =
        *reinterpret_cast<int32_t*>(
            movssAddress + 4);

    uintptr_t address =
        movssAddress + 8 + displacement;

    Log(
        "[Deadzone] Target address = %p\n",
        reinterpret_cast<void*>(address));

    float* deadzone =
        reinterpret_cast<float*>(address);

    Log(
        "[Deadzone] BEFORE WRITE = %f\n",
        *deadzone);

    DWORD oldProtect = 0;

    if (!VirtualProtect(
        deadzone,
        sizeof(float),
        PAGE_READWRITE,
        &oldProtect))
    {
        Log(
            "[Deadzone] VirtualProtect FAILED. Error = %lu\n",
            GetLastError());

        return 0;
    }

    *deadzone = 0.0f;

    Log(
        "[Deadzone] AFTER WRITE = %f\n",
        *deadzone);

    DWORD dummy = 0;

    VirtualProtect(
        deadzone,
        sizeof(float),
        oldProtect,
        &dummy);

    Log("[Deadzone] InitializeThread finished\n");

    return 0;
}

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        OutputDebugStringA(
            "[Deadzone] DLL_PROCESS_ATTACH\n");

        DisableThreadLibraryCalls(hModule);

        HANDLE thread =
            CreateThread(
                nullptr,
                0,
                InitializeThread,
                nullptr,
                0,
                nullptr);

        if (thread)
        {
            OutputDebugStringA(
                "[Deadzone] InitializeThread created\n");

            CloseHandle(thread);
        }
        else
        {
            char buffer[128];

            sprintf_s(
                buffer,
                "[Deadzone] CreateThread FAILED. Error = %lu\n",
                GetLastError());

            OutputDebugStringA(buffer);
        }
    }

    return TRUE;
}