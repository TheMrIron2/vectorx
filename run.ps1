param(
    [switch]$BuildOnly,
    [switch]$Test,
    [ValidateSet('Debug', 'Release')][string]$BuildType = 'Debug'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$previousPath = $env:PATH

function Move-LockedExecutable([string]$ExecutablePath) {
    if (-not (Test-Path -LiteralPath $ExecutablePath)) { return }
    $fileHandle = $null
    try {
        $fileHandle = [System.IO.File]::Open($ExecutablePath,
            [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    }
    catch {
        $cause = $_.Exception.GetBaseException()
        $nativeError = $cause.HResult -band 0xFFFF
        # Only sharing/lock violations indicate an in-use executable. Preserve
        # other errors (permissions, missing files, etc.) instead of hiding them.
        if ($cause -isnot [System.IO.IOException] -or $nativeError -notin @(32, 33)) { throw }
        $directory = Split-Path -Parent $ExecutablePath
        $backupName = 'vectorx-running-' + [guid]::NewGuid().ToString('N') + '.exe'
        $backupPath = Join-Path $directory $backupName
        # Windows permits renaming a running executable, but not overwriting it.
        # Both paths remain inside the selected build directory; keep the old
        # image available to its running process while the linker writes anew.
        Move-Item -LiteralPath $ExecutablePath -Destination $backupPath -ErrorAction Stop
        Write-Host "Executable is in use; preserved it as $backupName before rebuilding."
    }
    finally {
        if ($fileHandle) { $fileHandle.Dispose() }
    }
}

try {
    # Prefer the portable tools used to verify this checkout, if they are present.
    $portableCmake = Join-Path $PSScriptRoot 'build/tools/cmake-3.31.8-windows-x86_64/bin/cmake.exe'
    $portableNinja = Join-Path $PSScriptRoot 'build/tools/ninja/ninja.exe'
    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $portableCmake) { $cmakePath = $portableCmake }
    elseif ($cmakeCommand) { $cmakePath = $cmakeCommand.Source }
    else { throw 'CMake 3.24 or newer is required. See README.md.' }

    $ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
    $ninjaPath = $null
    if (Test-Path -LiteralPath $portableNinja) { $ninjaPath = $portableNinja }
    elseif ($ninjaCommand) { $ninjaPath = $ninjaCommand.Source }

    $buildDirectory = Join-Path $PSScriptRoot "build/$($BuildType.ToLowerInvariant())"
    $configure = @('-S', $PSScriptRoot, '-B', $buildDirectory, "-DCMAKE_BUILD_TYPE=$BuildType")
    if ($ninjaPath) {
        $configure += @('-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$($ninjaPath.Replace('\', '/'))")
        if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
            $gccCommand = Get-Command gcc.exe -ErrorAction SilentlyContinue
            $gccPath = if ($gccCommand) { $gccCommand.Source } else { 'C:\msys64\ucrt64\bin\gcc.exe' }
            if (Test-Path -LiteralPath $gccPath) {
                $compilerDirectory = Split-Path -Parent $gccPath
                $env:PATH = "$compilerDirectory;$env:PATH"
                $configure += "-DCMAKE_C_COMPILER=$($gccPath.Replace('\', '/'))"
                $resourceCompiler = Join-Path $compilerDirectory 'windres.exe'
                if (Test-Path -LiteralPath $resourceCompiler) {
                    $configure += "-DCMAKE_RC_COMPILER=$($resourceCompiler.Replace('\', '/'))"
                }
            }
        }
    }
    # An already downloaded SDL source is useful in restricted/offline sessions.
    $localSdl = Join-Path $PSScriptRoot 'build/SDL3-3.4.6'
    if (Test-Path -LiteralPath (Join-Path $localSdl 'CMakeLists.txt')) {
        $configure += "-DFETCHCONTENT_SOURCE_DIR_SDL3=$($localSdl.Replace('\', '/'))"
    }
    & $cmakePath @configure
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    $executable = Join-Path $buildDirectory 'vectorx.exe'
    $configurationExecutable = Join-Path $buildDirectory "$BuildType/vectorx.exe"
    if (-not (Test-Path -LiteralPath $executable) -and (Test-Path -LiteralPath $configurationExecutable)) {
        $executable = $configurationExecutable
    }
    Move-LockedExecutable $executable
    & $cmakePath --build $buildDirectory --config $BuildType --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if ($Test) {
        $ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
        & $ctestPath --test-dir $buildDirectory -C $BuildType --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'Checks failed.' }
    }
    if (-not $BuildOnly) {
        $executable = Join-Path $buildDirectory 'vectorx.exe'
        if (-not (Test-Path -LiteralPath $executable)) {
            $executable = Join-Path $buildDirectory "$BuildType/vectorx.exe"
        }
        Start-Process -FilePath $executable -WorkingDirectory (Split-Path -Parent $executable) -WindowStyle Hidden
    }
}
finally { $env:PATH = $previousPath }
