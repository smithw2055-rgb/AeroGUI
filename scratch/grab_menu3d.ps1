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

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static bool MaximizeAndCapture(IntPtr hWnd, string outputPath) {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        if (hDesk != IntPtr.Zero) {
            SetThreadDesktop(hDesk);
        }

        SetForegroundWindow(hWnd);
        // SW_MAXIMIZE = 3
        ShowWindow(hWnd, 3);
        System.Threading.Thread.Sleep(1000);

        RECT r;
        if (!GetWindowRect(hWnd, out r)) {
            Console.WriteLine("GetWindowRect failed: " + Marshal.GetLastWin32Error());
            if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
            return false;
        }

        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        Console.WriteLine("Maximized Window rect: " + w + "x" + h + " at (" + r.Left + "," + r.Top + ")");
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
'@

Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing

# Find PID of AeroMenu3D or launch if not running
$p = Get-Process AeroMenu3D -ErrorAction SilentlyContinue
if ($null -eq $p) {
    Write-Output "AeroMenu3D not running, starting it..."
    $proc = Start-Process -FilePath "C:\Projects\AeroGUI-R\build\samples\Menu3D\Release\AeroMenu3D.exe" -PassThru
    Start-Sleep -Seconds 2
}

# Find HWND on Default desktop
$finderSrc = @'
using System;
using System.Runtime.InteropServices;
using System.Text;

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

    public static IntPtr FindAeroWindow() {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        IntPtr found = IntPtr.Zero;
        EnumDesktopWindows(hDesk, (hWnd, lParam) => {
            StringBuilder cls = new StringBuilder(256);
            GetClassName(hWnd, cls, 256);
            if (cls.ToString() == "AeroGuiPlatformWindow") {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        CloseDesktop(hDesk);
        return found;
    }
}
'@
Add-Type -TypeDefinition $finderSrc

$hwnd = [DesktopFinder]::FindAeroWindow()
Write-Output "Aero HWND: 0x$($hwnd.ToString("X"))"

if ($hwnd -ne [IntPtr]::Zero) {
    $outPng = "C:\Projects\AeroGUI-R\build\menu3d_maximized.png"
    [DesktopScreenGrabber]::MaximizeAndCapture($hwnd, $outPng)
} else {
    Write-Output "No Aero window found!"
}
