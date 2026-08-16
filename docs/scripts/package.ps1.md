# scripts/package.ps1 — Packaging Script

## File role

Turns the built `r2dc.exe` into a distributable single-file artifact and
verifies the memory budget gate. Run after a successful build:

```powershell
./scripts/build.ps1
./scripts/package.ps1
```

## Steps

1. **Locate** `build/r2dc/Release/r2dc.exe`.
2. **Copy** it to `dist/r2dc.exe` (create `dist/` if missing).
3. **Verify size:** print EXE bytes; warn if on-disk size > 512 KB.
4. **Launch + measure working set** (best-effort, requires the app to run):
   - start process, wait 3 s,
   - read `(Get-Process r2dc).WorkingSet64`,
   - report MB; **fail the script** if > 5.0 MB (the hard budget).
   - kill the process afterwards.
5. Print a summary table and exit 0.

## Reference implementation

```powershell
param([string]$Config = "Release")

$ErrorActionPreference = "Stop"
$src = "build/r2dc/$Config/r2dc.exe"
if (-not (Test-Path $src)) { Write-Host "missing $src"; exit 1 }

New-Item -ItemType Directory -Force -Path dist | Out-Null
Copy-Item $src "dist/r2dc.exe" -Force

$size = (Get-Item "dist/r2dc.exe").Length
Write-Host ("dist/r2dc.exe: {0:N0} bytes ({1:N2} KB)" -f $size, ($size/1KB))
if ($size -gt 512KB) {
  Write-Warning "EXE larger than 512 KB — revisit /O1/LTCG flags."
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
```

## Notes

- `WorkingSet64` matches Task Manager's "Memory" column closely enough for the
  gate.
- The measurement run may briefly exceed 5 MB during startup; the script waits
  3 s for settling (matches [memory-budget.md](memory-budget.md)).
- No installer, no signing in v1. Optional future: `signtool`, README bundle,
  zip.

## Related

- [build.ps1.md](build.ps1.md) — must run first.
- [memory-budget.md](memory-budget.md) — defines the 5 MB gate and the
  per-component budget.
