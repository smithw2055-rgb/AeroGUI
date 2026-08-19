Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Image]::FromFile('c:\Projects\AeroGUI-R\build\rendered_login.bmp')
$bmp.Save('c:\Projects\AeroGUI-R\build\rendered_login.png', [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Copy-Item 'c:\Projects\AeroGUI-R\build\rendered_login.png' 'C:\Users\macx\.gemini\antigravity-ide\brain\f7818a3b-ade9-4cca-8681-54eb0325731d\rendered_login.png' -Force
