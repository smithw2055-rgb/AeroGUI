[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')

$src = @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public class InventoryLauncher {
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
    public static extern bool CloseHandle(IntPtr hObject);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr OpenDesktop(string lpszDesktop, uint dwFlags, bool fInherit, uint dwDesiredAccess);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool CloseDesktop(IntPtr hDesktop);

    [DllImport("user32.dll")]
    public static extern bool EnumDesktopWindows(IntPtr hDesktop, EnumWindowsProc lpfn, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    public static int Launch(string appPath, string desktop, string workDir) {
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
            return 0;
        }
        int pid = pi.dwProcessId;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return pid;
    }

    public static IntPtr FindWindow(string titleContains) {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        IntPtr found = IntPtr.Zero;
        EnumDesktopWindows(hDesk, (hWnd, lParam) => {
            StringBuilder title = new StringBuilder(256);
            GetWindowText(hWnd, title, 256);
            if (title.ToString().Contains(titleContains)) {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        CloseDesktop(hDesk);
        return found;
    }

    public static void BringToFront(IntPtr hWnd) {
        ShowWindow(hWnd, 9); // SW_RESTORE
        SetForegroundWindow(hWnd);
        SetWindowPos(hWnd, (IntPtr)(-1), 0, 0, 0, 0, 0x0001 | 0x0002); // HWND_TOPMOST, SWP_NOMOVE | SWP_NOSIZE
        SetWindowPos(hWnd, (IntPtr)(-2), 0, 0, 0, 0, 0x0001 | 0x0002); // HWND_NOTOPMOST, SWP_NOMOVE | SWP_NOSIZE
    }
}
'@

Add-Type -TypeDefinition $src

# 1. Stop any existing AeroInventory process
Stop-Process -Name AeroInventory -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

# 2. Launch on the interactive desktop WinSta0\Default
$appPath = "C:\Projects\AeroGUI-R\build\samples\Inventory\RelWithDebInfo\AeroInventory.exe"
$workDir = "C:\Projects\AeroGUI-R"

Write-Output "Launching $appPath on WinSta0\Default..."
$pidVal = [InventoryLauncher]::Launch($appPath, "WinSta0\Default", $workDir)
Write-Output "Launched AeroInventory with PID: $pidVal"

# 3. Wait for window to appear
Start-Sleep -Seconds 3

$hwnd = [InventoryLauncher]::FindWindow("Inventory")
Write-Output "AeroInventory HWND: 0x$($hwnd.ToString('X'))"

if ($hwnd -ne [IntPtr]::Zero) {
    [InventoryLauncher]::BringToFront($hwnd)
    Write-Output "Brought window to front on desktop."
} else {
    Write-Output "Window handle not found yet, but process $pidVal is running."
}
