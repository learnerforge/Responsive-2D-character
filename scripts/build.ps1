# One-command build + test driver. Configure, build Release, run CTest, print
# the EXE size. Run from the repo root:
#   ./scripts/build.ps1                       # MSVC (Visual Studio 2022)
#   ./scripts/build.ps1 -Generator Ninja      # MinGW-w64 (ucrt64 on PATH)
param(
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Arch = "Win32"
)

$ErrorActionPreference = "Stop"

$exe = "build/r2dc/Release/r2dc.exe"

$archArgs = @()
if ($Generator -ne "Ninja") { $archArgs = @("-A", $Arch) }

cmake -S . -B build -G $Generator @archArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (Test-Path $exe) {
  $size = (Get-Item $exe).Length
  Write-Host ("r2dc.exe: {0:N0} bytes ({1:N2} KB)" -f $size, ($size/1KB))
} elseif (Test-Path "build/r2dc.exe") {
  $size = (Get-Item "build/r2dc.exe").Length
  Write-Host ("r2dc.exe (Ninja layout): {0:N0} bytes ({1:N2} KB)" -f $size, ($size/1KB))
} else {
  Write-Host "ERROR: build artifact not found" -ForegroundColor Red
  exit 1
}
