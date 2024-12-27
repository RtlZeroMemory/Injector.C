using System;
using System.Runtime.InteropServices;

namespace InjectLib.Managed
{
    public static class DebugHelper
    {
        // 1) P/Invoke to kernel32.dll
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern void OutputDebugString(string lpOutputString);

        // 2) Simple wrapper method
        public static void Log(string message)
        {
            // Optionally, add timestamps or thread ID, etc.
            OutputDebugString($"[C# Managed] {message}");
        }
    }
}
