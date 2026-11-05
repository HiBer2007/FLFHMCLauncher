Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Runtime.InteropServices;
public class WinCap {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public static void Capture(IntPtr hwnd, string path) {
    RECT r; GetWindowRect(hwnd, out r);
    int w = r.Right - r.Left, h = r.Bottom - r.Top;
    using (var bmp = new Bitmap(w, h)) {
      using (var g = Graphics.FromImage(bmp)) {
        IntPtr hdc = g.GetHdc();
        PrintWindow(hwnd, hdc, 2);
        g.ReleaseHdc(hdc);
      }
      bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    }
  }
}
"@
$proc = Get-Process java | Sort-Object StartTime -Descending | Select-Object -First 1
[WinCap]::Capture([IntPtr]$proc.MainWindowHandle, 'K:\FLFHMCLauncher\video\hmcl.png')
Get-Item 'K:\FLFHMCLauncher\video\hmcl.png' | Select-Object Length
