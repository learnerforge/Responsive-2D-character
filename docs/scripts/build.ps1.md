# scripts/build.ps1 — Build Script

## File role

One-command build + test driver. Configure, build Release, run CTest, print the
EXE size. Designed to be run from the repo root:

```powershell
./scripts/build.ps1
```

## Contract

| Step | Action | Exit code |
|---|---|---|
| Configure | `cmake -S . -B build -G "Visual Studio 17 2022" -A Win32` | non-zero on failure |
| Build | `cmake --build build --config Release --parallel` | non-zero on failure |
| Test | `ctest --test-dir build -C Release --output-on-failure` | non-zero on test failure |
| Report | print path + size of `r2dc.exe` (bytes and KB) | 0 |

## PowerShell details

```powershell
$ErrorActionPreference = "Stop"

$exe = "build/r2dc/Release/r2dc.exe"
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (Test-Path $exe) {
  $size = (Get-Item $exe).Length
  Write-Host ("r2dc.exe: {0:N0} bytes ({1:N2} KB)" -f $size, ($size/1KB))
} else {
  Write-Host "ERROR: build artifact not found at $exe" -ForegroundColor Red
  exit 1
}
```

### Notes
- **Generator:** hardcoded to Visual Studio 17 2022 to keep the script trivial.
  Accept an optional `-Generator` parameter if you need flexibility.
- **Architecture:** `-A Win32` (32-bit) recommended — slightly smaller shared
  DLL tail and the app has no need for 64-bit addresses. Pass `-Arch x64` if you
  prefer.
- `$ErrorActionPreference = "Stop"` makes cmdlet errors fatal, but **native
  command failures need `$LASTEXITCODE` checks** (PowerShell 5.1 behavior) — the
  script checks after every native call.
- Prints EXE size so the memory budget gate (see
  [memory-budget.md](memory-budget.md)) can be sanity-checked from the shell.

## Parameters (optional, keep minimal)

```powershell
param(
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Arch = "Win32"
)
```

## Related

- [CMakeLists.txt.md](CMakeLists.txt.md) — what gets built.
- [package.ps1.md](package.ps1.md) — copy/strip/size-verify step after this
  succeeds.
