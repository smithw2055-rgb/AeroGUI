[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')

$source = @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Text;

public class ThreadWindows {
    public delegate bool EnumThreadDelegate(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumThreadWindows(int dwThreadId, EnumThreadDelegate lpfn, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Auto)]
    public static extern int GetClassName(IntPtr hWnd, StringBuilder lpClassName, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlink, uint nFlags);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public static IntPtr FindFirstWindow(int[] threadIds) {
        IntPtr found = IntPtr.Zero;
        foreach (int tid in threadIds) {
            EnumThreadWindows(tid, (hWnd, lParam) => {
                StringBuilder cls = new StringBuilder(256);
                GetClassName(hWnd, cls, 256);
                RECT r;
                GetWindowRect(hWnd, out r);
                if (IsWindowVisible(hWnd) && (r.Right - r.Left > 100)) {
                    found = hWnd;
                    return false;
                }
                return true;
            }, IntPtr.Zero);
            if (found != IntPtr.Zero) break;
        }
        return found;
    }

    public static bool CaptureWindow(IntPtr hWnd, string savePath) {
        RECT r;
        if (!GetWindowRect(hWnd, out r)) return false;
        int w = r.Right - r.Left;
        int h = r.Bottom - r.Top;
        if (w <= 0 || h <= 0) return false;

        SetForegroundWindow(hWnd);
        System.Threading.Thread.Sleep(500);

        using (Bitmap bmp = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(bmp)) {
                IntPtr hdc = g.GetHdc();
                bool pw = PrintWindow(hWnd, hdc, 2);
                g.ReleaseHdc(hdc);

                if (!pw) {
                    g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, h), CopyPixelOperation.SourceCopy);
                }
            }
            bmp.Save(savePath, ImageFormat.Png);
        }
        return true;
    }
}
"@

try {
    Add-Type -TypeDefinition $source -Language CSharp -ReferencedAssemblies System.Drawing
    $p = Get-Process AeroInventory -ErrorAction Stop
    $tids = [int[]]($p.Threads | ForEach-Object { $_.Id })
    $hwnd = [ThreadWindows]::FindFirstWindow($tids)
    Write-Output "Found HWND: 0x$($hwnd.ToString("X"))"
    if ($hwnd -ne [IntPtr]::Zero) {
        $savePath = "C:\Projects\AeroGUI-R\build\inventory_rendered.png"
        $ok = [ThreadWindows]::CaptureWindow($hwnd, $savePath)
        Write-Output "Captured to $savePath : $ok"
    }
} catch {
    Write-Error $_
}
