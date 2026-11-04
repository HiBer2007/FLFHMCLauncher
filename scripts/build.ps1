param(
    [string]$QtRoot = "H:/Qt-static-rt/6.11.1/msvc2022_64",
    [string]$VcpkgRoot = "H:/vcpkg",
    [string]$BuildDir = "build",
    [string]$Triplet = "x64-windows-static",
    [string]$VcVars = "F:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path $VcVars)) { throw "vcvars64.bat not found: $VcVars" }
cmd /c "`"$VcVars`" >nul && cmake -B $BuildDir -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=`"$QtRoot`" -DCMAKE_TOOLCHAIN_FILE=`"$VcpkgRoot/scripts/buildsystems/vcpkg.cmake`" -DVCPKG_TARGET_TRIPLET=$Triplet"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
cmd /c "`"$VcVars`" >nul && cmake --build $BuildDir --config Release"
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
