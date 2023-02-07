#include <node.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <stdio.h>
#include <malloc.h>

#ifndef UNICODE
#define UNICODE
#define UNICODE_WAS_UNDEFINED
#endif

#include <Windows.h>

#ifdef UNICODE_WAS_UNDEFINED
#undef UNICODE
#endif

struct SystemInformation {
    int cpu_usage;
    DWORD dwMemoryLoad;
    DWORDLONG  ullTotalPhys;
    DWORDLONG ullTotalPageFile;
    DWORDLONG ullAvailPageFile;
    std::string hard_drives;
};

uint64_t FromFileTime(const FILETIME& ft) {

    ULARGE_INTEGER uli = { 0 };
    
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    return uli.QuadPart;

}


SystemInformation getSystemInformation() {

    SystemInformation info;

    // Cpu usage

    FILETIME idle_time, kernel_time, user_time;

    GetSystemTimes(&idle_time, &kernel_time, &user_time);

    uint64_t k1 = FromFileTime(kernel_time);
    uint64_t u1 = FromFileTime(user_time);
    uint64_t i1 = FromFileTime(idle_time);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    GetSystemTimes(&idle_time, &kernel_time, &user_time);

    uint64_t k2 = FromFileTime(kernel_time);
    uint64_t u2 = FromFileTime(user_time);
    uint64_t i2 = FromFileTime(idle_time);


    uint64_t ker = k2 - k1;
    uint64_t usr = u2 - u1;
    uint64_t idl = i2 - i1;

    uint64_t cpu = (ker + usr - idl) * 100 / (ker + usr);

    info.cpu_usage = static_cast<int>(cpu);

    // Memory usage

    unsigned long long installedMemory = 0;
    GetPhysicallyInstalledSystemMemory(&installedMemory);

    MEMORYSTATUSEX memstatus; memstatus.dwLength = sizeof(memstatus);
    GlobalMemoryStatusEx(&memstatus);

    info.dwMemoryLoad = memstatus.dwMemoryLoad; // Percentage
    info.ullTotalPhys = ((memstatus.ullTotalPhys / 1024) / 1024); // MB
    info.ullAvailPageFile = ((memstatus.ullAvailPageFile / 1024) / 1024); // MB
    info.ullTotalPageFile = ((memstatus.ullTotalPageFile / 1024) / 1024); // MB

    // Hard disks

    DWORD cchBuffer;
    WCHAR* driveStrings;
    UINT driveType;
    PWSTR driveTypeString;
    ULARGE_INTEGER freeSpace;

    // Find out how big a buffer we need
    cchBuffer = GetLogicalDriveStrings(0, NULL);

    driveStrings = (WCHAR*)malloc((cchBuffer + 1) * sizeof(TCHAR));

    if (driveStrings != NULL)
    {

        std::wstring drives;
        
        // Fetch all drive strings    
        GetLogicalDriveStrings(cchBuffer, driveStrings);

        // Loop until we find the final '\0'
        // driveStrings is a double null terminated list of null terminated strings)
        wchar_t* singleDriveString = driveStrings;
        while (*singleDriveString)
        {
            // Dump drive information
            driveType = GetDriveType(singleDriveString);
            GetDiskFreeSpaceEx(singleDriveString, &freeSpace, NULL, NULL);

            switch (driveType)
            {
            case DRIVE_FIXED:

                drives = drives + singleDriveString[0] + L": " + L"HDD" + L" " + std::to_wstring(freeSpace.QuadPart / 1024 / 1024 / 1024) + L" "; // GB

                break;


            case DRIVE_REMOVABLE:

                drives = drives + singleDriveString[0] + L": " + L"REM" + L" " + std::to_wstring(freeSpace.QuadPart / 1024 / 1024 / 1024) + L" "; // GB

                break;

            case DRIVE_REMOTE:

                drives = drives + singleDriveString[0] + L": " + L"NET" + L" " + std::to_wstring(freeSpace.QuadPart / 1024 / 1024 / 1024) + L" "; // GB

                break;

            default:
                break;
            }
        
            singleDriveString += lstrlen(singleDriveString) + 1;
        }

        info.hard_drives = std::string(drives.begin(), drives.end());

    }

    free(driveStrings);

    return info;

}

namespace agent {

    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::NewStringType;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void Method(const FunctionCallbackInfo<Value>& args) {

        SystemInformation info = getSystemInformation();

        std::string info_str = "[cpu]: " + std::to_string(info.cpu_usage) + "% [ram]: " + std::to_string(info.dwMemoryLoad) + "% [memory]: " + std::to_string(info.ullTotalPhys) + " [pagefile]: " + std::to_string(info.ullAvailPageFile) + "/" + std::to_string(info.ullTotalPageFile) + " [drives]: " + info.hard_drives;

        Isolate* isolate = args.GetIsolate();
        v8::MaybeLocal<v8::String> str = String::NewFromUtf8(isolate, info_str.c_str(), NewStringType::kNormal);
        v8::Local<v8::String> checkedString = str.ToLocalChecked();
        v8::ReturnValue<v8::Value> retVal = args.GetReturnValue();
        retVal.Set(checkedString);
    }

    void Initialize(Local<Object> exports) {
    NODE_SET_METHOD(exports, "getSystemInformation", Method);
    }

    NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}