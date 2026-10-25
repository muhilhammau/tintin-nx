# build.ps1 -- PowerShell version of runtime/tools/docker_build.sh
# usage: .\build.ps1 | .\build.ps1 clean | .\build.ps1 rt-files | .\build.ps1 DCR_GL_MESA=0
param([Parameter(ValueFromRemainingArguments=$true)] [string[]]$MakeArgs)
$ErrorActionPreference = 'Stop'

$here  = $PSScriptRoot
$libnx = if ($env:DCR_LIBNX32) { $env:DCR_LIBNX32 } else { Join-Path $here '..\libnx32\prefix' }
$libnx = (Resolve-Path $libnx).Path
$img   = if ($env:DCR_TOOLCHAIN_IMAGE) { $env:DCR_TOOLCHAIN_IMAGE } else { 'ghcr.io/vita2hos/devcontainer/vita2hos:latest' }
$nxd   = '/opt/devkitpro/libnx32'

foreach ($f in 'lib\libnx.a','include\switch.h') {
  if (-not (Test-Path (Join-Path $libnx $f))) { throw "libnx32 prefix tidak lengkap: $libnx ($f hilang)" }
}
if (-not (Test-Path "$here\Makefile") -or -not (Test-Path "$here\runtime\runtime.mk")) {
  throw "$here bukan folder port (Makefile atau runtime\runtime.mk tidak ada)"
}
if (-not (Test-Path "$here\portlibs32\lib\libEGL.a")) { Write-Warning "portlibs32 tidak ada: build pakai null renderer" }

# string perintah tanpa tanda kutip di dalamnya (PowerShell 5.1 merusaknya)
$cmd = 'exec make -j$(nproc) ' + ($MakeArgs -join ' ')

docker run --rm --platform linux/amd64 `
  -v "${here}:/work" -w /work `
  -v "${libnx}\include\switch:${nxd}/include/switch:ro" `
  -v "${libnx}\include\switch.h:${nxd}/include/switch.h:ro" `
  -v "${libnx}\lib\libnx.a:${nxd}/lib/libnx.a:ro" `
  -v "${libnx}\lib\libnxd.a:${nxd}/lib/libnxd.a:ro" `
  $img bash -lc $cmd
if ($LASTEXITCODE -ne 0) { throw "build gagal (exit $LASTEXITCODE)" }
