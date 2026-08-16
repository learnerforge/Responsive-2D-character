# Turns the built r2dc.exe into a distributable single-file artifact and
# verifies the memory budget gate. Run after a successful build:
#   ./scripts/build.ps1
#   ./scripts/package.ps1
param([string]$Config = "Release")

$ErrorActionPreference = "Stop"

$candidates = @("build/r2dc/$Config/r2dc.exe",   # MSVC generator layout
                "build/r2dc.exe")                # Ninja / single-config layout
$src = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $src) {
  Write-Host "missing r2dc.exe (looked for: $($candidates -join ', '))"
  exit 1
}

New-Item -ItemType Directory -Force -Path dist | Out-Null
Copy-Item $src "dist/r2dc.exe" -Force

$size = (Get-Item "dist/r2dc.exe").Length
Write-Host ("dist/r2dc.exe: {0:N0} bytes ({1:N2} KB)" -f $size, ($size/1KB))
if ($size -gt 512KB) {
  Write-Warning ("EXE larger than 512 KB - revisit /O1/LTCG flags (MSVC). " +
                 "MinGW -static builds are larger by design; see docs/revisions.md R9.")
}

$p = Start-Process -FilePath "dist/r2dc.exe" -PassThru
Start-Sleep -Seconds 3
$ws = (Get-Process -Id $p.Id).WorkingSet64
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue

$mb = $ws / 1MB
Write-Host ("Working set: {0:N2} MB" -f $mb)
if ($mb -gt 5.0) {
  Write-Error "Memory budget exceeded (limit 5.0 MB). See docs/memory-budget.md"
  exit 1
}
exit 0
