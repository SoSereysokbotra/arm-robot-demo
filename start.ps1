# One command to bring the whole thing up:
#
#     .\start.ps1
#
# Starts the MQTT broker, serves the control page over HTTP so phones on the
# same Wi-Fi can open it too, and launches it in your browser.
#
#     .\start.ps1 -Stop        shut everything down
#     .\start.ps1 -Shortcut    put a launcher on the Desktop

param(
    [switch]$Stop,
    [switch]$Shortcut,
    [int]$Port = 8000
)

$proj = $PSScriptRoot
$mosq = "C:\Program Files\mosquitto\mosquitto.exe"
$conf = Join-Path $proj "nodered\mosquitto.conf"
$simDir = Join-Path $proj "sim"

function Get-ServerProcs {
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -like '*http.server*' -and $_.CommandLine -like "*$Port*" }
}

# ---------------------------------------------------------------- shortcut --
if ($Shortcut) {
    $lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Robot Arm.lnk'
    $sh = New-Object -ComObject WScript.Shell
    $s = $sh.CreateShortcut($lnk)
    $s.TargetPath = 'powershell.exe'
    $s.Arguments = "-NoExit -ExecutionPolicy Bypass -File `"$(Join-Path $proj 'start.ps1')`""
    $s.WorkingDirectory = $proj
    $s.IconLocation = 'shell32.dll,13'
    $s.Description = 'Start the robot arm broker and control page'
    $s.Save()
    Write-Host "Shortcut created: $lnk" -ForegroundColor Green
    Write-Host "Double-click it to start everything."
    return
}

# -------------------------------------------------------------------- stop --
if ($Stop) {
    Get-ServerProcs | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process mosquitto -ErrorAction SilentlyContinue |
        Where-Object { $_.SessionId -ne 0 } |          # leave the Windows service alone
        Stop-Process -Force -ErrorAction SilentlyContinue
    Write-Host "Stopped." -ForegroundColor Yellow
    return
}

# ------------------------------------------------------------------ broker --
$brokerUp = Get-NetTCPConnection -State Listen -LocalPort 1883 -ErrorAction SilentlyContinue |
            Where-Object { $_.LocalAddress -eq '0.0.0.0' }
if ($brokerUp) {
    Write-Host "[1/3] Mosquitto already running." -ForegroundColor DarkGray
} else {
    Write-Host "[1/3] Starting Mosquitto..."
    Start-Process $mosq -ArgumentList '-c', $conf -WindowStyle Minimized
    Start-Sleep -Seconds 2
}

# ------------------------------------------------------------- web server --
# Serving over HTTP (rather than opening the file directly) is what lets a
# phone on the same Wi-Fi reach the page.
if (Get-ServerProcs) {
    Write-Host "[2/3] Page server already running." -ForegroundColor DarkGray
} else {
    Write-Host "[2/3] Serving the page on port $Port..."
    Start-Process python -ArgumentList '-m', 'http.server', $Port, '--directory', $simDir -WindowStyle Minimized
    Start-Sleep -Seconds 2
}

# -------------------------------------------------------------------- open --
# Take the address of the adapter that actually has a default gateway -
# otherwise WSL and Hyper-V virtual adapters win and the phone URL is useless.
$ip = (Get-NetIPConfiguration -ErrorAction SilentlyContinue |
       Where-Object { $_.IPv4DefaultGateway -and $_.NetAdapter.Status -eq 'Up' } |
       Select-Object -First 1).IPv4Address.IPAddress

Write-Host "[3/3] Opening the control page..."
Start-Process "http://localhost:$Port/arm3d.html?host=localhost"

# ----------------------------------------------------------------- report --
$ports = Get-NetTCPConnection -State Listen -LocalPort 1883, 9001 -ErrorAction SilentlyContinue |
         Select-Object -ExpandProperty LocalPort -Unique | Sort-Object
Write-Host ""
if ($ports -contains 1883 -and $ports -contains 9001) {
    Write-Host "Broker listening on 1883 and 9001." -ForegroundColor Green
} else {
    Write-Host "WARNING: expected ports 1883 and 9001, got: $($ports -join ', ')" -ForegroundColor Red
    Write-Host "Check nodered\mosquitto.conf has the websockets listener."
}
if ($ip) {
    Write-Host "On this laptop :  http://localhost:$Port/arm3d.html"
    Write-Host "From a phone   :  http://${ip}:$Port/arm3d.html?host=$ip"
    Write-Host "                  (same Wi-Fi, and set BROKER to $ip)"
}
Write-Host "Stop everything with:  .\start.ps1 -Stop" -ForegroundColor DarkGray
