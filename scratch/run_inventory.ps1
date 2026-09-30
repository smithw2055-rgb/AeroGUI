[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')

$src = @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public class DesktopLauncher {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct STARTUPINFO {
        public int cb;
        public string lpReserved;
        public string lpDesktop;
        public string lpTitle;
        public int dwX;
        public int dwY;
        public int dwXSize;
        public int dwYSize;
        public int dwXCountChars;
        public int dwYCountChars;
        public int dwFillAttribute;
        public int dwFlags;
        public short wShowWindow;
        public short cbReserved2;
        public IntPtr lpReserved2;
        public IntPtr hStdInput;
        public IntPtr hStdOutput;
        public IntPtr hStdError;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PROCESS_INFORMATION {
        public IntPtr hProcess;
        public IntPtr hThread;
        public int dwProcessId;
        public int dwThreadId;
    }

    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool CreateProcess(
        string lpApplicationName,
        string lpCommandLine,
        IntPtr lpProcessAttributes,
        IntPtr lpThreadAttributes,
        bool bInheritHandles,
        uint dwCreationFlags,
        IntPtr lpEnvironment,
        string lpCurrentDirectory,
        ref STARTUPINFO lpStartupInfo,
        out PROCESS_INFORMATION lpProcessInformation);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr hHandle, uint dwMilliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetExitCodeProcess(IntPtr hProcess, out uint lpExitCode);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr hObject);

    public static void LaunchAndWait(string appPath, string desktop, string workDir) {
        STARTUPINFO si = new STARTUPINFO();
        si.cb = Marshal.SizeOf(si);
        si.lpDesktop = desktop;
        si.dwFlags = 1; // STARTF_USESHOWWINDOW
        si.wShowWindow = 1; // SW_SHOWNORMAL

        PROCESS_INFORMATION pi = new PROCESS_INFORMATION();
        bool ok = CreateProcess(null, "\"" + appPath + "\"", IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, workDir, ref si, out pi);
        if (!ok) {
            int err = Marshal.GetLastWin32Error();
            Console.WriteLine("CreateProcess failed with error: " + err);
            return;
        }
        Console.WriteLine("Process created: PID=" + pi.dwProcessId + " on desktop '" + desktop + "'");
        
        while (true) {
            uint waitRes = WaitForSingleObject(pi.hProcess, 1000);
            if (waitRes == 0) {
                uint exitCode;
                GetExitCodeProcess(pi.hProcess, out exitCode);
                Console.WriteLine("Process exited with code: " + exitCode);
                break;
            }
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}
'@

Add-Type -TypeDefinition $src
[DesktopLauncher]::LaunchAndWait("C:\Projects\AeroGUI-R\build\samples\Inventory\Release\AeroInventory.exe", "WinSta0\Default", "C:\Projects\AeroGUI-R")
