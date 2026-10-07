param(
    [switch]$BuildOnly,
    [switch]$Test,
    [ValidateSet('Debug', 'Release')][string]$BuildType = 'Debug'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$previousPath = $env:PATH

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
    & $cmakePath --build $buildDirectory --config $BuildType --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if ($Test) {
        $ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
        & $ctestPath --test-dir $buildDirectory -C $BuildType --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'Core checks failed.' }
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
