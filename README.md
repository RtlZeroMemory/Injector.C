# Injector.C

DLL Injection with CoreCLR and Managed Plugins

This project demonstrates how to extend native Windows processes using DLL injection and .NET Core (CoreCLR). It injects a native DLL into a target process, initializes the CoreCLR runtime, and executes managed C# code. This approach can be used to create a plugin system or extend the functionality of existing native applications.

How It Works

Native DLL Injection:

The injector program starts by injecting a native DLL into a target process (e.g., notepad.exe).
The DLL uses Windows API functions like VirtualAllocEx, WriteProcessMemory, and CreateRemoteThread to load itself into the process's memory space.
Loading CoreCLR:

Once injected, the DLL initializes the .NET Core runtime (CoreCLR) inside the target process.
It dynamically loads the managed assembly (InjectLib.Managed.dll) containing the C# code.
Executing Managed Code:

The DLL creates a delegate to the managed entry point (Main method) and invokes it.
The managed code can interact with the process, display message boxes, or execute plugins.

Extending the Process:

The managed code can act as a plugin host, dynamically loading and executing additional plugins at runtime.
