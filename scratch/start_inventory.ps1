$env:TEMP = "C:\Users\macx\AppData\Local\Temp"
$env:TMP = "C:\Users\macx\AppData\Local\Temp"

# Stop any running instances first
Stop-Process -Name AeroInventory -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

# Start AeroInventory directly on user desktop
$exe = "C:\Projects\AeroGUI-R\build\samples\Inventory\RelWithDebInfo\AeroInventory.exe"
$workDir = "C:\Projects\AeroGUI-R"

$proc = Start-Process -FilePath $exe -WorkingDirectory $workDir -PassThru
Write-Output "Started AeroInventory with PID=$($proc.Id)"

Start-Sleep -Seconds 2

$p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
if ($p -and !$p.HasExited) {
    Write-Output "AeroInventory is running successfully (PID=$($p.Id))."
} else {
    Write-Output "Process exited unexpectedly."
}
