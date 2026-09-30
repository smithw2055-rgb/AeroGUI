[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')
Add-Type -AssemblyName System.Drawing

$src = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public class ScreenCap {
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
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetThreadDesktop(IntPtr hDesktop);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left, Top, Right, Bottom;
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

    public static bool Capture(IntPtr hWnd, string outputPath) {
        IntPtr hDesk = OpenDesktop("Default", 0, false, 0x10000000);
        if (hDesk != IntPtr.Zero) SetThreadDesktop(hDesk);

        ShowWindow(hWnd, 9); // SW_RESTORE
        SetForegroundWindow(hWnd);
        System.Threading.Thread.Sleep(300);

        RECT r;
        GetWindowRect(hWnd, out r);
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        Console.WriteLine(string.Format("Rect: {0},{1} to {2},{3} ({4}x{5})", r.Left, r.Top, r.Right, r.Bottom, w, h));

        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
            }
            bmp.Save(outputPath, ImageFormat.Png);
        }
        if (hDesk != IntPtr.Zero) CloseDesktop(hDesk);
        return true;
    }
}
'@

Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing

$hwnd = [ScreenCap]::FindInventoryWindow()
Write-Output "HWND: $hwnd"
if ($hwnd -ne [IntPtr]::Zero) {
    $out = "C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\screen_cap.png"
    [ScreenCap]::Capture($hwnd, $out)
    Write-Output "Saved to $out"
}
