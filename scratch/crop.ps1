[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')
Add-Type -AssemblyName System.Drawing

$file = 'C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\inventory_hover.png'
$bmp = [System.Drawing.Bitmap]::FromFile($file)
Write-Output "Size: $($bmp.Width) x $($bmp.Height)"
$rect = [System.Drawing.Rectangle]::FromLTRB(600, 0, $bmp.Width, $bmp.Height)
$crop = $bmp.Clone($rect, $bmp.PixelFormat)
$out = 'C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\inventory_right.png'
$crop.Save($out)
$bmp.Dispose()
$crop.Dispose()
Write-Output "Saved crop to $out"
