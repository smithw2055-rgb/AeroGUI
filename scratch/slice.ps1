[System.Environment]::SetEnvironmentVariable('TEMP', 'C:\Users\macx\AppData\Local\Temp')
[System.Environment]::SetEnvironmentVariable('TMP', 'C:\Users\macx\AppData\Local\Temp')
Add-Type -AssemblyName System.Drawing

$file = 'C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\inventory_fresh.png'
$bmp = [System.Drawing.Bitmap]::FromFile($file)
Write-Output "Full size: $($bmp.Width) x $($bmp.Height)"

$rect1 = [System.Drawing.Rectangle]::FromLTRB(0, 0, 500, $bmp.Height)
$bmp.Clone($rect1, $bmp.PixelFormat).Save('C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\part_left.png')

$rect2 = [System.Drawing.Rectangle]::FromLTRB(500, 0, 1000, $bmp.Height)
$bmp.Clone($rect2, $bmp.PixelFormat).Save('C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\part_mid.png')

$rect3 = [System.Drawing.Rectangle]::FromLTRB(1000, 0, $bmp.Width, $bmp.Height)
$bmp.Clone($rect3, $bmp.PixelFormat).Save('C:\Users\macx\.gemini\antigravity-ide\brain\6802036f-36a1-4baf-877e-95c5c9ae18b9\part_right.png')

$bmp.Dispose()
Write-Output "Done"
