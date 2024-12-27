using System.Diagnostics;
using System.Runtime.InteropServices;

namespace InjectLib.Managed
{
 /*
 * Summary:
 * This managed C# class (`InjectLibManaged`) is the main entry point for a .NET Core assembly 
 * intended to be injected into a native process. It performs the following tasks:
 * 
 * 1. **Retrieve Process Information**:
 *    - Uses the `System.Diagnostics` namespace to get details about the current process (name and ID).
 * 
 * 2. **Display MessageBox**:
 *    - Invokes the Win32 `MessageBox` function to display a message box in the context of the injected process.
 *    - The message box includes the process name and ID to verify the context of the injection.
 * 
 * 3. **Thread Sleep for Observation**:
 *    - Keeps the managed thread alive for 50 seconds using `Thread.Sleep`, allowing time for debugging 
 *      or observing the effects of the injection.
 */

    public static class InjectLibManaged
    {
        const uint MB_OK = 0x00000000;
        const uint MB_ICONFIRMATION = 0x00000040; 

        [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        public static extern int MessageBox(IntPtr hWnd, string text, string caption, uint type);

        public static void Main()
        {
            var proc = Process.GetCurrentProcess();
            MessageBox(
                IntPtr.Zero,
                $"Hello from Process with Id: {proc.Id}, {proc.ProcessName}",
                "Hello from injected DLL",
                MB_OK | MB_ICONFIRMATION
            );

            Thread.Sleep(50000);
        }
    }
}
