#pragma once
#include <iostream>
#include <windows.h>
// Ensure the DLL file name matches your compiled DLL
const char* DLLAssetPath = "../ExternalLibs/Unigma/";

// Function to convert a const char* to a std::wstring
std::wstring ConvertCharToWString(const char* charArray) {
    size_t len = strlen(charArray) + 1;
    wchar_t* wString = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, charArray, -1, wString, len);
    std::wstring result(wString);
    delete[] wString; // Free allocated memory
    return result;
}

std::wstring GetAbsolutePath(const std::wstring& relativePath) {
    wchar_t absolutePath[MAX_PATH];
    GetFullPathNameW(relativePath.c_str(), MAX_PATH, absolutePath, nullptr);
    return std::wstring(absolutePath);
}

HMODULE LoadDLL(const wchar_t* dllName)
{
    // Convert DLLAssetPath to std::wstring and concatenate
    std::wstring wDLLAssetPath = ConvertCharToWString(DLLAssetPath);
    std::wstring relativePath = wDLLAssetPath + dllName;

    std::wcout << L"Loading DLL from relative path: " << relativePath << std::endl;

    wchar_t originalDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, originalDir);

    //Set default directory.
    SetCurrentDirectoryW(wDLLAssetPath.c_str());

    // Load the DLL using the relative path
    HMODULE hModule = LoadLibrary(relativePath.c_str());
    if (hModule == NULL) {

        DWORD errorCode = GetLastError();
        LPWSTR errorMsg = nullptr;

        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&errorMsg,
            0,
            nullptr
        );

        std::wcerr << L"Failed to load DLL: " << relativePath << std::endl;
        std::wcerr << L"Error code: " << errorCode << L", Message: " << (errorMsg ? errorMsg : L"Unknown error") << std::endl;

        return hModule;
    }

    std::wcout << L"Successfully loaded DLL: " << relativePath << std::endl;

    // Restore original working directory
    SetCurrentDirectory(originalDir);
    return hModule;

}