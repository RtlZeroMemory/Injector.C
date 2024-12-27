using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using Injector.Managed;

namespace CSharpInjector
{
    /// <summary>
    /// This program demonstrates how to inject a DLL into a target process using the Windows API. 
    /// Here's how it works:
    /// 
    /// 1. **Launch Target Process**:
    ///    - Starts the target process (e.g., `notepad.exe`) in a suspended state so it can be modified before execution.
    ///
    /// 2. **Allocate Memory**:
    ///    - Reserves memory in the target process to store the path of the DLL being injected.
    ///
    /// 3. **Write DLL Path**:
    ///    - Copies the path of the DLL into the allocated memory in the target process.
    ///
    /// 4. **Find LoadLibraryA**:
    ///    - Locates the address of the `LoadLibraryA` function, which will load the DLL into the process.
    ///
    /// 5. **Create Remote Thread**:
    ///    - Starts a thread in the target process that calls `LoadLibraryA` with the DLL path as its argument.
    ///
    /// 6. **Wait and Resume**:
    ///    - Waits for the remote thread to finish loading the DLL and then resumes the main thread of the target process.
    ///
    /// 7. **Clean Up**:
    ///    - Releases allocated resources like process and thread handles. If any step fails, it terminates the process and reports the error.
    /// 
    /// This program is a practical example of remote DLL injection, with built-in error handling and resource management.
    /// </summary>

    internal class Program
    {
        static void Main(string[] args)
        {
            // Native C DLL path to inject, responsible for loading CLR into the target process
            string dllPath = @"C:\Users\k3nig\source\repos\Injector.C\x64\Debug\InjectLib.Native.dll";

            Win32Interop.STARTUPINFO si = new Win32Interop.STARTUPINFO();
            si.cb = (uint)Marshal.SizeOf(si);

            Win32Interop.PROCESS_INFORMATION pi;
            bool success = Win32Interop.CreateProcessA(
                @"C:\Windows\System32\notepad.exe", //Process to inject 
                null,
                IntPtr.Zero,
                IntPtr.Zero,
                false,
                Win32Interop.CREATE_SUSPENDED,
                IntPtr.Zero,
                null,
                ref si,
                out pi);

            if (!success)
            {
                Console.WriteLine("CreateProcess failed. Error = {0}", Marshal.GetLastWin32Error());
                return;
            }

            Console.WriteLine("[+] Process started in suspended mode (PID={0})", pi.dwProcessId);

            try
            {
                byte[] dllBytes = Encoding.ASCII.GetBytes(dllPath + "\0");
                IntPtr remoteAddr = Win32Interop.VirtualAllocEx(
                    pi.hProcess,
                    IntPtr.Zero,
                    (IntPtr)dllBytes.Length,
                    Win32Interop.MEM_COMMIT_RESERVE,
                    Win32Interop.PAGE_READWRITE);

                if (remoteAddr == IntPtr.Zero)
                {
                    Console.WriteLine("VirtualAllocEx failed. Error = {0}", Marshal.GetLastWin32Error());
                    throw new Exception("VirtualAllocEx failed.");
                }

                if (!Win32Interop.WriteProcessMemory(pi.hProcess, remoteAddr, dllBytes, dllBytes.Length, out _))
                {
                    Console.WriteLine("WriteProcessMemory failed. Error = {0}", Marshal.GetLastWin32Error());
                    throw new Exception("WriteProcessMemory failed.");
                }

                IntPtr hKernel32 = Win32Interop.GetModuleHandle("kernel32.dll");
                if (hKernel32 == IntPtr.Zero)
                {
                    Console.WriteLine("GetModuleHandle(kernel32.dll) failed. Error = {0}", Marshal.GetLastWin32Error());
                    throw new Exception("GetModuleHandle failed.");
                }
                IntPtr pLoadLibraryA = Win32Interop.GetProcAddress(hKernel32, "LoadLibraryA");
                if (pLoadLibraryA == IntPtr.Zero)
                {
                    Console.WriteLine("GetProcAddress(LoadLibraryA) failed. Error = {0}", Marshal.GetLastWin32Error());
                    throw new Exception("GetProcAddress failed.");
                }

                IntPtr hRemoteThread = Win32Interop.CreateRemoteThread(
                    pi.hProcess,
                    IntPtr.Zero,
                    IntPtr.Zero,
                    pLoadLibraryA,
                    remoteAddr,
                    0,
                    out _);

                if (hRemoteThread == IntPtr.Zero)
                {
                    Console.WriteLine("CreateRemoteThread failed. Error = {0}", Marshal.GetLastWin32Error());
                    throw new Exception("CreateRemoteThread failed.");
                }

                Console.WriteLine("[+] Remote thread created to load the DLL.");
                Win32Interop.WaitForSingleObject(hRemoteThread, Win32Interop.WAIT_INFINITE);
                Win32Interop.CloseHandle(hRemoteThread);

                uint ret = Win32Interop.ResumeThread(pi.hThread);
                if (ret == unchecked((uint)-1))
                {
                    Console.WriteLine("ResumeThread failed. Error = {0}", Marshal.GetLastWin32Error());
                    throw new Exception("ResumeThread failed.");
                }

                Console.WriteLine("[+] DLL injected, target process is now running.");
            }
            catch (Exception ex)
            {
                Win32Interop.TerminateProcess(pi.hProcess, 0);
                Console.WriteLine($"[!] Injection failed: {ex.Message}. Process terminated.");
            }
            finally
            {
                Win32Interop.CloseHandle(pi.hThread);
                Win32Interop.CloseHandle(pi.hProcess);
            }
        }
    }
}
