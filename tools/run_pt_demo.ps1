# Launch mock PT demo host tools against a 2CH Kvaser (ch0=vehicle, ch1=cluster).
# Flash the stm32_h5 full-glue image first (AEP_DEMO_BOARD_UDS=0).
param(
    [string]$Interface = "kvaser",
    [int]$ChVehicle = 0,
    [int]$ChCluster = 1
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "Starting PT gen + UDS PID ECU on channel $ChVehicle ..."
$p1 = Start-Process -FilePath "python" -ArgumentList @(
    "tools/pt_can_gen.py", "--interface", $Interface, "--channel", "$ChVehicle"
) -PassThru -NoNewWindow
$p2 = Start-Process -FilePath "python" -ArgumentList @(
    "tools/uds_pid_ecu.py", "--interface", $Interface, "--channel", "$ChVehicle"
) -PassThru -NoNewWindow

Start-Sleep -Seconds 1
Write-Host "Sniffing cluster on channel $ChCluster (Ctrl+C to stop sniff; then stop generators) ..."
try {
    python tools/cluster_sniff.py --interface $Interface --channel $ChCluster
}
finally {
    foreach ($p in @($p1, $p2)) {
        if ($p -and -not $p.HasExited) {
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        }
    }
}
