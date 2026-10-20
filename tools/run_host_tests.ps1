# Run portable library host tests with gcc (MinGW). No make required.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Ensure-Dir($p) {
    New-Item -ItemType Directory -Force -Path $p | Out-Null
}

Ensure-Dir sig/test/build
Ensure-Dir ucp/test/build
Ensure-Dir vcom/test/build
Ensure-Dir vcom_diag/test/build

Write-Host '=== sig ==='
gcc -std=c11 -Wall -Wextra -Werror -Isig/include `
    -o sig/test/build/test_sig.exe `
    sig/src/sig_catalog.c sig/src/sig_tlv.c sig/src/sig_table.c sig/test/test_sig.c
& sig/test/build/test_sig.exe

Write-Host '=== ucp ==='
gcc -std=c11 -Wall -Wextra -Werror -Iucp/include -Iaep_log/include -DAEP_LOG_ENABLE=0 `
    -o ucp/test/build/test_ucp_roles.exe `
    ucp/src/ucp_protocol.c ucp/src/ucp_parser.c ucp/src/ucp_port.c `
    ucp/src/ucp_session.c ucp/src/ucp_session_debug.c aep_log/src/aep_log_port.c `
    ucp/test/test_ucp_roles.c
& ucp/test/build/test_ucp_roles.exe

Write-Host '=== vcom ==='
gcc -std=c11 -Wall -Wextra -Werror -Ivcom/include `
    -o vcom/test/build/test_vcom.exe `
    vcom/src/vcom_core.c vcom/src/vcom_rx_demux.c vcom/test/test_vcom.c
& vcom/test/build/test_vcom.exe

Write-Host '=== vcom_diag ==='
gcc -std=c11 -Wall -Wextra -Wno-unused-parameter `
    -Ivcom_diag/include -Ivcom/include -Iaep_log/include -Ivcom_diag/third_party/iso14229 `
    -DUDS_SYS=0 -DUDS_TP_ISOTP_C -DAEP_LOG_ENABLE=0 `
    -o vcom_diag/test/build/test_diag_link.exe `
    vcom/src/vcom_core.c vcom_diag/src/diag_link.c aep_log/src/aep_log_port.c `
    vcom_diag/third_party/iso14229/iso14229.c vcom_diag/test/test_diag_link.c
& vcom_diag/test/build/test_diag_link.exe

Write-Host 'ALL TESTS PASSED'
