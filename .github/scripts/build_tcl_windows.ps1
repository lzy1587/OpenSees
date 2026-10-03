param(
    [Parameter(Mandatory = $true)][string]$VsPath,
    [Parameter(Mandatory = $true)][string]$OutDir
)
$ErrorActionPreference = 'Stop'
$deps = Join-Path $env:RUNNER_TEMP 'vsfsib-tcl-tk-8.6.18'
New-Item -ItemType Directory -Force -Path $deps | Out-Null
$tcl = Join-Path $deps 'tcl'
$tk = Join-Path $deps 'tk'
git clone --depth 1 --branch core-8-6-18 https://github.com/tcltk/tcl.git $tcl
if ($LASTEXITCODE -ne 0) { throw 'TCL DEPENDENCY FAILURE: official Tcl clone failed' }
git clone --depth 1 --branch core-8-6-18 https://github.com/tcltk/tk.git $tk
if ($LASTEXITCODE -ne 0) { throw 'TCL DEPENDENCY FAILURE: official Tk clone failed' }
"Tcl tag: $(git -C $tcl describe --tags) commit $(git -C $tcl rev-parse HEAD)" |
    Set-Content (Join-Path $OutDir 'TCL_SOURCE_INFO.txt')
"Tk tag: $(git -C $tk describe --tags) commit $(git -C $tk rev-parse HEAD)" |
    Add-Content (Join-Path $OutDir 'TCL_SOURCE_INFO.txt')
$script = Join-Path $PSScriptRoot 'build_tcl_tk.cmd'
& $script $VsPath $tcl $tk 2>&1 | Tee-Object -FilePath (Join-Path $OutDir 'TCL_BUILD.log')
if ($LASTEXITCODE -ne 0) { throw 'TCL DEPENDENCY FAILURE: official Tcl/Tk nmake build failed' }
$include = 'C:\Program Files\tcl\include\tcl.h'
$tclLib = 'C:\Program Files\tcl\lib\tcl86t.lib'
$tkLib = 'C:\Program Files\tcl\lib\tk86t.lib'
foreach ($file in @($include, $tclLib, $tkLib)) {
    if (-not (Test-Path $file)) { throw "TCL DEPENDENCY FAILURE: missing installed $file" }
}
$zlib = Join-Path $tcl 'compat\zlib\win64\zlib1.dll'
if (-not (Test-Path $zlib)) { throw 'TCL DEPENDENCY FAILURE: bundled x64 zlib1.dll missing' }
Copy-Item -LiteralPath $zlib -Destination 'C:\Program Files\tcl\bin\zlib1.dll'
