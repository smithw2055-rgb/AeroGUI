[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')

$src = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public class QuestTester2 {
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
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlink, uint nFlags);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
        public POINT(int x, int y) { X = x; Y = y; }
    }

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int X, int Y);

    public static int Launch(string exePath, string desktop, string workDir) {
        STARTUPINFO si = new STARTUPINFO();
        si.cb = Marshal.SizeOf(si);
        si.lpDesktop = desktop;
        si.dwFlags = 1;
        si.wShowWindow = 1;

        PROCESS_INFORMATION pi = new PROCESS_INFORMATION();
        bool ok = CreateProcess(null, "\"" + exePath + "\"", IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, workDir, ref si, out pi);
        if (!ok) {
            Console.WriteLine("CreateProcess failed: " + Marshal.GetLastWin32Error());
            return 0;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return pi.dwProcessId;
    }

    public static IntPtr FindQuestWindow() {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        IntPtr found = IntPtr.Zero;
        EnumDesktopWindows(hDesk, (hWnd, lParam) => {
            StringBuilder title = new StringBuilder(256);
            GetWindowText(hWnd, title, 256);
            if (title.ToString().Contains("QuestLog")) {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        CloseDesktop(hDesk);
        return found;
    }

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetThreadDesktop(IntPtr hDesktop);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [DllImport("user32.dll")]
    public static extern bool SetProcessDPIAware();

    public static bool CaptureWindow(IntPtr hWnd, string outputPath) {
        SetProcessDPIAware();
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        if (hDesk != IntPtr.Zero) {
            SetThreadDesktop(hDesk);
        }
        ShowWindow(hWnd, 9);
        System.Threading.Thread.Sleep(500);

        RECT r;
        if (!GetWindowRect(hWnd, out r)) {
            if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
            return false;
        }
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        if (w <= 0 || h <= 0) {
            if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
            return false;
        }

        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc();
                bool pw = PrintWindow(hWnd, hdc, 2);
                g.ReleaseHdc(hdc);
                if (!pw) {
                    IntPtr hdc2 = g.GetHdc();
                    PrintWindow(hWnd, hdc2, 0);
                    g.ReleaseHdc(hdc2);
                }
            }
            bmp.Save(outputPath, ImageFormat.Png);
            Console.WriteLine("Saved: " + outputPath);
        }
        if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
        return true;
    }

    public static void HoverThumb(IntPtr hWnd) {
        POINT pt = new POINT(296, 250);
        ClientToScreen(hWnd, ref pt);
        SetCursorPos(pt.X, pt.Y);
    }
}
'@

Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing

Get-Process AeroQuestLog -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

$exePath = "C:\Projects\AeroGUI-R\build\samples\QuestLog\RelWithDebInfo\AeroQuestLog.exe"
if (-not (Test-Path $exePath)) {
    $exePath = "C:\Projects\AeroGUI-R\build\samples\QuestLog\Debug\AeroQuestLog.exe"
}
$newPid = [QuestTester2]::Launch($exePath, "WinSta0\Default", "C:\Projects\AeroGUI-R")
Write-Output "Launched PID: $newPid"
Start-Sleep -Seconds 3

$hwnd = [QuestTester2]::FindQuestWindow()
Write-Output "QuestLog HWND: 0x$($hwnd.ToString('X'))"

if ($hwnd -ne [IntPtr]::Zero) {
    $outPath = "C:\Users\macx\.gemini\antigravity-ide\brain\b29e9f73-8b84-43cd-bba7-8b1287c5e001\questlog_result.png"
    [QuestTester2]::CaptureWindow($hwnd, $outPath)
    Write-Output "Captured default to $outPath"

    [QuestTester2]::HoverThumb($hwnd)
    Start-Sleep -Milliseconds 800

    $outHover = "C:\Users\macx\.gemini\antigravity-ide\brain\b29e9f73-8b84-43cd-bba7-8b1287c5e001\questlog_hover.png"
    [QuestTester2]::CaptureWindow($hwnd, $outHover)
    Write-Output "Captured hover to $outHover"
} else {
    Write-Output "Window not found!"
}

Stop-Process -Id $newPid -Force
