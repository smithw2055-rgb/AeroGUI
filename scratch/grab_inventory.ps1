[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')

$src = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public class DesktopScreenGrabber {
    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr OpenDesktop(string lpszDesktop, uint dwFlags, bool fInherit, uint dwDesiredAccess);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetThreadDesktop(IntPtr hDesktop);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool CloseDesktop(IntPtr hDesktop);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlink, uint nFlags);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static bool Capture(IntPtr hWnd, string outputPath) {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        if (hDesk != IntPtr.Zero) {
            SetThreadDesktop(hDesk);
        }

        SetForegroundWindow(hWnd);
        System.Threading.Thread.Sleep(500);

        RECT r;
        if (!GetWindowRect(hWnd, out r)) {
            Console.WriteLine("GetWindowRect failed: " + Marshal.GetLastWin32Error());
            if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
            return false;
        }

        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        Console.WriteLine("Window rect: " + w + "x" + h + " at (" + r.Left + "," + r.Top + ")");
        if (w <= 0 || h <= 0) {
            if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
            return false;
        }

        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc();
                bool pw = PrintWindow(hWnd, hdc, 2);
                g.ReleaseHdc(hdc);
                Console.WriteLine("PrintWindow returned: " + pw);

                if (!pw) {
                    try {
                        g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
                        Console.WriteLine("CopyFromScreen succeeded");
                    } catch (Exception ex) {
                        Console.WriteLine("CopyFromScreen error: " + ex.Message);
                    }
                }
            }
            bmp.Save(outputPath, ImageFormat.Png);
            Console.WriteLine("Saved screenshot to " + outputPath);
        }

        if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
        return true;
    }
}

public class DesktopFinder {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern IntPtr OpenDesktop(string lpszDesktop, uint dwFlags, bool fInherit, uint dwDesiredAccess);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool CloseDesktop(IntPtr hDesktop);
    [DllImport("user32.dll")]
    public static extern bool EnumDesktopWindows(IntPtr hDesktop, EnumWindowsProc lpfn, IntPtr lParam);
    [DllImport("user32.dll")]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    public static IntPtr FindWindowByPid(uint targetPid) {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        IntPtr found = IntPtr.Zero;
        EnumDesktopWindows(hDesk, (hWnd, lParam) => {
            uint pid;
            GetWindowThreadProcessId(hWnd, out pid);
            if (pid == targetPid) {
                StringBuilder cls = new StringBuilder(256);
                GetClassName(hWnd, cls, 256);
                if (cls.ToString() == "AeroGuiPlatformWindow") {
                    found = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        CloseDesktop(hDesk);
        return found;
    }
}
'@

Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing

$hwnd = [IntPtr]0x480658
Write-Output "AeroInventory HWND: 0x$($hwnd.ToString("X"))"

if ($hwnd -ne [IntPtr]::Zero) {
    $outPng = "C:\Projects\AeroGUI-R\build\inventory_rendered.png"
    [DesktopScreenGrabber]::Capture($hwnd, $outPng)
} else {
    Write-Output "No Aero window found!"
}
