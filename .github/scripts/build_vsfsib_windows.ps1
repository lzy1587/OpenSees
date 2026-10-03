$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Set-Location $root
$out = Join-Path $root 'ci_out'
New-Item -ItemType Directory -Force -Path $out | Out-Null

$expected = '5c925e6a24bf3fbda58e57dde32cde658b3c5d5b'
git merge-base --is-ancestor $expected HEAD
if ($LASTEXITCODE -ne 0) { throw 'SOURCE REGISTRATION FAILURE: checkout is not based on official v3.3.0' }
if (-not (Select-String -Path 'SRC\classTags.h' -Pattern '#define MAT_TAG_VSFSIBBreakaway 6114' -Quiet)) {
    throw 'SOURCE REGISTRATION FAILURE: custom classTag missing'
}
if (-not (Select-String -Path 'Win64\proj\material\material.vcxproj' -Pattern 'VSFSIBBreakaway.cpp' -Quiet)) {
    throw 'VCXPROJ INCLUSION FAILURE: custom source missing from material project'
}

$pyAudit = Join-Path $out 'PYTHON_ABI_AUDIT.txt'
python --version 2>&1 | Tee-Object -FilePath $pyAudit
where.exe python | Tee-Object -FilePath $pyAudit -Append
python -c "import sys,sysconfig; print('executable=',sys.executable); print('prefix=',sys.prefix); print('base_prefix=',sys.base_prefix); print('include=',sysconfig.get_paths()['include']); print('LIBDIR=',sysconfig.get_config_var('LIBDIR')); print('LDLIBRARY=',sysconfig.get_config_var('LDLIBRARY')); print('architecture=',8*__import__('struct').calcsize('P'))" | Tee-Object -FilePath $pyAudit -Append
$prefix = (python -c "import sys; print(sys.prefix)").Trim()
$pyInclude = Join-Path $prefix 'include'
$pyLib = Join-Path $prefix 'libs'
if (-not (Test-Path (Join-Path $pyInclude 'Python.h')) -or -not (Test-Path (Join-Path $pyLib 'python38.lib'))) {
    throw 'PYTHON ABI FAILURE: Python 3.8 headers/import library missing'
}
Get-Item (Join-Path $pyInclude 'Python.h'), (Join-Path $pyLib 'python38.lib') | Select-Object FullName,Length | Out-String | Add-Content $pyAudit

$project = 'Win64\proj\openSeesPy38\OpenSeesPy38.vcxproj'
$original = Get-Content -LiteralPath $project -Raw
$fromInclude = 'c:\Program Files\Python38\include'
$fromLib = 'c:\Program Files\Python38\libs'
if (-not $original.Contains($fromInclude) -or -not $original.Contains($fromLib)) {
    throw 'PYTHON ABI FAILURE: unexpected v3.3.0 vcxproj Python paths'
}
Copy-Item -LiteralPath $project -Destination (Join-Path $out 'OpenSeesPy38.original.vcxproj')
Set-Content -LiteralPath $project -Value $original.Replace($fromInclude, $pyInclude).Replace($fromLib, $pyLib) -NoNewline
git diff -- $project | Set-Content (Join-Path $out 'github_python_path_patch.diff')
[xml](Get-Content -LiteralPath $project -Raw) | Out-Null

$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'MSVC COMPILE FAILURE: vswhere not found' }
$vs = (& $vswhere -latest -products '*' -property installationPath).Trim()
if (-not $vs) { throw 'MSVC COMPILE FAILURE: Visual Studio not found' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path $msbuild)) { throw 'MSVC COMPILE FAILURE: MSBuild not found' }
& (Join-Path $PSScriptRoot 'build_tcl_windows.ps1') -VsPath $vs -OutDir $out
@("Source commit: $(git rev-parse HEAD)", "Base tag: v3.3.0 ($expected)",
  "Runner: $env:RUNNER_OS / windows-2022", "Visual Studio: $vs",
  "MSBuild: $(& $msbuild -version -nologo | Select-Object -Last 1)",
  "Python: $(python --version)", "Python include: $pyInclude", "Python libs: $pyLib",
  'Configuration: Release', 'Platform: x64', 'ClassTag: 6114',
  "Build time (UTC): $([DateTime]::UtcNow.ToString('u'))") |
    Set-Content (Join-Path $out 'BUILD_INFO.txt')

$log = Join-Path $out 'MSBUILD_FULL.log'
& $msbuild $project '/m' '/p:Configuration=Release' '/p:Platform=x64' '/v:diagnostic' '/nologo' '/fl' "/flp:logfile=$log;verbosity=diagnostic" 2>&1 |
    Tee-Object -FilePath (Join-Path $out 'MSBUILD_CONSOLE.log')
$buildExit = $LASTEXITCODE
if ($buildExit -ne 0) {
    $lines = Get-Content -LiteralPath $log -ErrorAction SilentlyContinue
    if (-not $lines) { $lines = Get-Content (Join-Path $out 'MSBUILD_CONSOLE.log') }
    $first = $lines | Select-String -Pattern 'fatal error|error C[0-9]+|error LNK[0-9]+|error MSB[0-9]+' | Select-Object -First 1
    $firstCompile = $lines | Select-String -Pattern 'fatal error|error C[0-9]+' | Select-Object -First 1
    $firstLink = $lines | Select-String -Pattern 'error LNK[0-9]+' | Select-Object -First 1
    $classification = if ($firstLink) { 'E: LINK FAILURE' } else { 'D: MSVC COMPILE FAILURE' }
    @("Classification: $classification", "MSBuild exit code: $buildExit", "First diagnostic: $first",
      "First compile error: $firstCompile", "First LNK error: $firstLink") |
        Set-Content (Join-Path $out 'BUILD_FAILURE_SUMMARY.txt')
    throw "MSVC COMPILE OR LINK FAILURE: see $log"
}

$pyd = Join-Path $root 'Win64\bin\opensees.pyd'
if (-not (Test-Path $pyd)) { throw 'PYD PACKAGING FAILURE: build exited 0 but opensees.pyd is absent' }
Copy-Item 'C:\Program Files\tcl\bin\tcl86t.dll' (Split-Path $pyd)
Copy-Item 'C:\Program Files\tcl\bin\tk86t.dll' (Split-Path $pyd)
$binary = Get-Item $pyd
$hash = (Get-FileHash -LiteralPath $pyd -Algorithm SHA256).Hash
@("Binary path: $($binary.FullName)", "Size: $($binary.Length) bytes", "SHA256: $hash") |
    Add-Content (Join-Path $out 'BUILD_INFO.txt')
$env:VSFSIB_CUSTOM_PYD = $pyd
$env:VSFSIB_SMOKE_CSV = Join-Path $out 'CI_MATERIAL_SMOKE_TEST.csv'
python 'validation\test_import_and_command.py' 2>&1 |
    Tee-Object -FilePath (Join-Path $out 'IMPORT_AND_SMOKE_TEST.txt')
if ($LASTEXITCODE -ne 0) { throw 'IMPORT, COMMAND REGISTRATION, OR MATERIAL RUNTIME FAILURE' }
