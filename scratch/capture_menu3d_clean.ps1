$env:TEMP = "C:\Users\macx\AppData\Local\Temp"
$env:TMP = "C:\Users\macx\AppData\Local\Temp"

$src = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public class WindowGrabber {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlink, uint nFlags);

    public static IntPtr FindWindowForPid(uint targetPid) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hWnd, lParam) => {
            uint pid = 0;
            GetWindowThreadProcessId(hWnd, out pid);
            if (pid == targetPid && IsWindowVisible(hWnd)) {
                StringBuilder title = new StringBuilder(256);
                GetWindowText(hWnd, title, 256);
                StringBuilder cls = new StringBuilder(256);
                GetClassName(hWnd, cls, 256);
                if (title.Length > 0 || cls.ToString().Contains("Aero")) {
                    found = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    public static bool Capture(IntPtr hWnd, string outputPath) {
        RECT r;
        if (!GetWindowRect(hWnd, out r)) return false;
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        if (w <= 0 || h <= 0) return false;

        SetForegroundWindow(hWnd);
        System.Threading.Thread.Sleep(500);

        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                // Try PrintWindow first with PW_RENDERFULLCONTENT (2)
                IntPtr hdc = g.GetHdc();
                bool pw = PrintWindow(hWnd, hdc, 2);
                g.ReleaseHdc(hdc);

                if (!pw) {
                    // fallback to CopyFromScreen
                    g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
                }
            }
            bmp.Save(outputPath, ImageFormat.Png);
        }
        return true;
    }
}
"@

Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing

# Start AeroMenu3D
$proc = Start-Process -FilePath "C:\Projects\AeroGUI-R\build\samples\Menu3D\Release\AeroMenu3D.exe" -PassThru
Write-Output "Started AeroMenu3D with PID=$($proc.Id)"

$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 300
    $hwnd = [WindowGrabber]::FindWindowForPid($proc.Id)
    if ($hwnd -ne [IntPtr]::Zero) { break }
}

Write-Output "Found HWND: 0x$($hwnd.ToString("X"))"

if ($hwnd -ne [IntPtr]::Zero) {
    Start-Sleep -Seconds 1
    $outPng = "C:\Projects\AeroGUI-R\build\menu3d_rendered.png"
    $ok = [WindowGrabber]::Capture($hwnd, $outPng)
    Write-Output "Capture result: $ok, path=$outPng"
} else {
    Write-Output "Could not find window!"
}

# Note: leave process running or kill? Let's check:
# If user wants to see it, we can leave it running.
