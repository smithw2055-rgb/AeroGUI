[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')

$src = @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public class WinFocus {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    public static void FocusPid(uint targetPid) {
        EnumWindows((hWnd, lParam) => {
            uint pid = 0;
            GetWindowThreadProcessId(hWnd, out pid);
            if (pid == targetPid) {
                ShowWindow(hWnd, 9); // SW_RESTORE
                SetForegroundWindow(hWnd);
                SetWindowPos(hWnd, (IntPtr)(-1), 0, 0, 0, 0, 0x0001 | 0x0002);
                SetWindowPos(hWnd, (IntPtr)(-2), 0, 0, 0, 0, 0x0001 | 0x0002);
                Console.WriteLine("Focused HWND: 0x" + hWnd.ToString("X"));
                return false;
            }
            return true;
        }, IntPtr.Zero);
    }
}
'@

Add-Type -TypeDefinition $src

$p = Get-Process -Name AeroInventory -ErrorAction SilentlyContinue
if ($p) {
    [WinFocus]::FocusPid($p.Id)
    Write-Output "Brought AeroInventory (PID $($p.Id)) to the front."
} else {
    Write-Output "Process not found."
}
