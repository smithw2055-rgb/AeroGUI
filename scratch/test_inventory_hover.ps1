[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')

$src = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public class InventoryTester2 {
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
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int X, int Y);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlink, uint nFlags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetThreadDesktop(IntPtr hDesktop);

    public const uint WM_MOUSEMOVE = 0x0200;
    public const uint WM_LBUTTONDOWN = 0x0201;
    public const uint WM_LBUTTONUP = 0x0202;

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left, Top, Right, Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X, Y;
    }

    public static int Launch(string exePath, string desktop, string workDir) {
        STARTUPINFO si = new STARTUPINFO();
        si.cb = Marshal.SizeOf(si);
        si.lpDesktop = desktop;
        si.dwFlags = 1;
        si.wShowWindow = 1;

        PROCESS_INFORMATION pi = new PROCESS_INFORMATION();
        bool ok = CreateProcess(null, "\"" + exePath + "\"", IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, workDir, ref si, out pi);
        if (!ok) return 0;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return pi.dwProcessId;
    }

    public static IntPtr FindInventoryWindow() {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        IntPtr found = IntPtr.Zero;
        EnumDesktopWindows(hDesk, (hWnd, lParam) => {
            StringBuilder title = new StringBuilder(256);
            GetWindowText(hWnd, title, 256);
            if (title.ToString().Contains("Inventory")) {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        CloseDesktop(hDesk);
        return found;
    }

    public static bool PositionWindow(IntPtr hWnd) {
        return SetWindowPos(hWnd, IntPtr.Zero, 50, 50, 1280, 720, 0x0040);
    }

    public static void SendMouseMove(IntPtr hWnd, int clientX, int clientY) {
        IntPtr lParam = (IntPtr)((clientY << 16) | (clientX & 0xFFFF));
        PostMessage(hWnd, WM_MOUSEMOVE, IntPtr.Zero, lParam);
    }

    public static void SendMouseDown(IntPtr hWnd, int clientX, int clientY) {
        IntPtr lParam = (IntPtr)((clientY << 16) | (clientX & 0xFFFF));
        PostMessage(hWnd, WM_LBUTTONDOWN, (IntPtr)1, lParam);
    }

    public static void SendMouseUp(IntPtr hWnd, int clientX, int clientY) {
        IntPtr lParam = (IntPtr)((clientY << 16) | (clientX & 0xFFFF));
        PostMessage(hWnd, WM_LBUTTONUP, IntPtr.Zero, lParam);
    }

    public static bool CaptureWindow(IntPtr hWnd, string outputPath) {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        if (hDesk != IntPtr.Zero) SetThreadDesktop(hDesk);

        RECT r;
        GetWindowRect(hWnd, out r);
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;

        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc();
                bool pw = PrintWindow(hWnd, hdc, 2);
                g.ReleaseHdc(hdc);
                if (!pw) {
                    try {
                        g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
                    } catch {}
                }
            }
            bmp.Save(outputPath, ImageFormat.Png);
        }
        if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
        return true;
    }
}
'@

Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing

$p = Get-Process AeroInventory -ErrorAction SilentlyContinue
if ($null -eq $p) {
    $newPid = [InventoryTester2]::Launch("C:\Projects\AeroGUI-R\build\samples\Inventory\RelWithDebInfo\AeroInventory.exe", "WinSta0\Default", "C:\Projects\AeroGUI-R")
    Write-Output "Launched PID: $newPid"
    Start-Sleep -Seconds 7
}

$hwnd = [InventoryTester2]::FindInventoryWindow()
if ($hwnd -eq [IntPtr]::Zero) {
    Write-Output "Window not found!"
    exit 1
}

Write-Output "Positioning window..."
[InventoryTester2]::PositionWindow($hwnd)
Start-Sleep -Milliseconds 500

# Let's hover over inventory slot 0.
# Inventory panel is on the right side. In a 1280x720 window:
# Inventory column starts at ~730px.
# First slot is at roughly X=760, Y=220.
Write-Output "Hovering over slot 0 at (760, 220)..."
[InventoryTester2]::SendMouseMove($hwnd, 760, 220)
Start-Sleep -Milliseconds 1000

$out = "C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\inventory_hover.png"
[InventoryTester2]::CaptureWindow($hwnd, $out)
Write-Output "Captured hover to $out"
