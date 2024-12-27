#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/**
 * Summary:
 * This native DLL hosts the .NET Core runtime (CoreCLR) inside a target process. Here's what it does:
 *
 * 1. **Starts on Injection**:
 *    - When the DLL is loaded, it kicks off a new thread (`StartCoreCLR`) to handle the runtime setup.
 *
 * 2. **Sets Up CoreCLR**:
 *    - Loads `coreclr.dll` dynamically and resolves essential functions like `coreclr_initialize`,
 *      `coreclr_create_delegate`, and `coreclr_shutdown`.
 *
 * 3. **Builds a TPA List**:
 *    - Scans the .NET runtime folder for necessary assemblies and adds them to a list (Trusted Platform Assemblies or TPA).
 *    - Includes your custom managed assembly (`InjectLib.Managed.dll`) to make it accessible to the runtime.
 *
 * 4. **Initializes the Runtime**:
 *    - Sets up the runtime with the TPA list and required paths, preparing it to run managed code.
 *
 * 5. **Runs Managed Code**:
 *    - Creates a delegate to the `Main` method in `InjectLib.Managed.InjectLibManaged` and runs it.
 *
 * 6. **Cleans Up**:
 *    - Keeps the runtime alive while the managed code runs, then gracefully shuts down CoreCLR and releases resources.
 *
 * 7. **Logs Activity**:
 *    - Outputs debug messages to `OutputDebugString`, which you can view with tools like Sysinternals DebugView.
 *
 * Use Case:
 * This DLL is designed to be injected into a process, where it sets up .NET Core and runs your managed code.
 * Make sure to adjust file paths for your runtime and assemblies to match your environment.
 */


typedef int (*coreclr_initialize_ptr)(
    const char* exePath,
    const char* appDomainFriendlyName,
    int propertyCount,
    const char** propertyKeys,
    const char** propertyValues,
    void** hostHandle,
    unsigned int* domainId
    );

typedef int (*coreclr_create_delegate_ptr)(
    void* hostHandle,
    unsigned int domainId,
    const char* assemblyName,
    const char* typeName,
    const char* methodName,
    void** delegate
    );

typedef int (*coreclr_shutdown_ptr)(
    void* hostHandle,
    unsigned int domainId
    );

typedef void (*ManagedEntryPoint)(void);

static void DebugLog(const char* fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    OutputDebugStringA(buffer);
}

static void BuildTPAList(const char* runtimeFolder, const char* userAssemblyPath, char* tpaList, size_t tpaListSize)
{
    ZeroMemory(tpaList, tpaListSize);

    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s*.dll", runtimeFolder);

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(searchPath, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        DebugLog("[InjectedDLL] WARNING: Could not find *.dll in folder: %s\n", runtimeFolder);
        return;
    }

    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char fullDllPath[MAX_PATH];
            snprintf(fullDllPath, sizeof(fullDllPath), "%s%s", runtimeFolder, ffd.cFileName);
            strncat(tpaList, fullDllPath, tpaListSize - strlen(tpaList) - 1);
            strncat(tpaList, ";", tpaListSize - strlen(tpaList) - 1);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);

    strncat(tpaList, userAssemblyPath, tpaListSize - strlen(tpaList) - 1);
    strncat(tpaList, ";", tpaListSize - strlen(tpaList) - 1);
    DebugLog("[InjectedDLL] Final TPA list:\n%s\n", tpaList);
}

static DWORD WINAPI StartCoreCLR(LPVOID lpParam)
{
    DebugLog("[InjectedDLL] StartCoreCLR thread started.\n");

    const char* runtimeFolder = "C:\\Program Files\\dotnet\\shared\\Microsoft.NETCore.App\\8.0.11\\";
    const char* userAssemblyPath = "C:\\Users\\k3nig\\source\\repos\\Injector.C\\InjectLib.Managed\\bin\\Debug\\net8.0\\InjectLib.Managed.dll";

    char tpaList[32768];
    BuildTPAList(runtimeFolder, userAssemblyPath, tpaList, sizeof(tpaList));

    DebugLog("[InjectedDLL] Loading coreclr.dll...\n");
    char coreclrPath[MAX_PATH];
    snprintf(coreclrPath, sizeof(coreclrPath), "%scoreclr.dll", runtimeFolder);

    HMODULE hCoreCLR = LoadLibraryA(coreclrPath);
    if (!hCoreCLR) {
        DWORD err = GetLastError();
        DebugLog("[InjectedDLL] ERROR: LoadLibraryA for coreclr.dll failed. GLE=%lu\n", err);
        MessageBoxA(NULL, "Failed to load coreclr.dll!", "InjectedDLL", MB_OK | MB_ICONERROR);
        return 1;
    }
    DebugLog("[InjectedDLL] coreclr.dll loaded at %p\n", (void*)hCoreCLR);

    coreclr_initialize_ptr coreclr_initialize =
        (coreclr_initialize_ptr)GetProcAddress(hCoreCLR, "coreclr_initialize");
    coreclr_create_delegate_ptr coreclr_create_delegate =
        (coreclr_create_delegate_ptr)GetProcAddress(hCoreCLR, "coreclr_create_delegate");
    coreclr_shutdown_ptr coreclr_shutdown =
        (coreclr_shutdown_ptr)GetProcAddress(hCoreCLR, "coreclr_shutdown");

    if (!coreclr_initialize || !coreclr_create_delegate || !coreclr_shutdown) {
        DebugLog("[InjectedDLL] ERROR: Could not get required CoreCLR exports.\n");
        MessageBoxA(NULL, "Failed to get CoreCLR exports!", "InjectedDLL", MB_OK | MB_ICONERROR);
        FreeLibrary(hCoreCLR);
        return 2;
    }

    const char* propertyKeys[] = {
        "TRUSTED_PLATFORM_ASSEMBLIES",
        "APP_PATHS",
        "NATIVE_DLL_SEARCH_DIRECTORIES"
    };

    const char* appPaths = "C:\\Users\\k3nig\\source\\repos\\Injector.C\\InjectLib.Managed\\bin\\Debug\\net8.0";
    const char* nativeDllPaths = "C:\\Users\\k3nig\\source\\repos\\Injector.C\\InjectLib.Managed\\bin\\Debug\\net8.0";

    const char* propertyValues[] = {
        tpaList,
        appPaths,
        nativeDllPaths
    };

    DebugLog("[InjectedDLL] Calling coreclr_initialize...\n");
    void* hostHandle = NULL;
    unsigned int domainId = 0;
    int hr = coreclr_initialize(
        "notepad.exe",  
        "DefaultDomain",
        3,
        propertyKeys,
        propertyValues,
        &hostHandle,
        &domainId
    );

    if (hr < 0 || !hostHandle) {
        DebugLog("[InjectedDLL] ERROR: coreclr_initialize failed, HR=%d\n", hr);
        MessageBoxA(NULL, "coreclr_initialize failed!", "InjectedDLL", MB_OK | MB_ICONERROR);
        FreeLibrary(hCoreCLR);
        return 3;
    }
    DebugLog("[InjectedDLL] coreclr_initialize succeeded. HostHandle=%p, DomainId=%u\n", hostHandle, domainId);

    DebugLog("[InjectedDLL] Creating delegate for 'InjectLib.Managed.InjectLibManaged.Main'...\n");
    void* pDelegate = NULL;
    hr = coreclr_create_delegate(
        hostHandle,
        domainId,
        "InjectLib.Managed",                
        "InjectLib.Managed.InjectLibManaged",
        "Main",                            
        &pDelegate
    );

    if (hr < 0 || !pDelegate) {
        DebugLog("[InjectedDLL] ERROR: coreclr_create_delegate failed, HR=%d\n", hr);
        MessageBoxA(NULL, "Failed to create delegate!", "InjectedDLL", MB_OK | MB_ICONERROR);
        coreclr_shutdown(hostHandle, domainId);
        FreeLibrary(hCoreCLR);
        return 4;
    }
    DebugLog("[InjectedDLL] Delegate created successfully at %p\n", pDelegate);

    DebugLog("[InjectedDLL] Invoking managed entry point...\n");
    ManagedEntryPoint entryPoint = (ManagedEntryPoint)pDelegate;
    entryPoint();

    DebugLog("[InjectedDLL] Managed entry point finished. Waiting for runtime to exit...\n");
    WaitForSingleObject(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entryPoint, NULL, 0, NULL), INFINITE);
    DebugLog("[InjectedDLL] Managed runtime has exited.\n");

    DebugLog("[InjectedDLL] Shutting down CoreCLR...\n");
    coreclr_shutdown(hostHandle, domainId);

    FreeLibrary(hCoreCLR);
    DebugLog("[InjectedDLL] CoreCLR shutdown completed. DLL thread exiting.\n");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DebugLog("[InjectedDLL] DllMain => DLL_PROCESS_ATTACH. Starting CLR thread.\n");
        CreateThread(NULL, 0, StartCoreCLR, NULL, 0, NULL);
        break;

    case DLL_PROCESS_DETACH:
        DebugLog("[InjectedDLL] DllMain => DLL_PROCESS_DETACH. Unloading...\n");
        break;
    }
    return TRUE;
}
