# 把 GUI / quickstart / spdlog / VC runtime 追加进 FLFHMCLauncher.exe。
# 生成的 EXE 首次运行会释放到 <exe>/.flfhmcl/bin，再启动 GUI。
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$OutputExe = ""
)

$ErrorActionPreference = "Stop"
$BuildDir = (Resolve-Path $BuildDir).Path
if (-not $OutputExe) { $OutputExe = Join-Path $BuildDir "FLFHMCLauncher.exe" }

function Require-File([string]$Path) {
    if (-not (Test-Path $Path -PathType Leaf)) { throw "缺少文件: $Path" }
    return (Resolve-Path $Path).Path
}

$bootstrap = Require-File (Join-Path $BuildDir "FLFHMCLauncherBootstrap.exe")
$app = Require-File (Join-Path $BuildDir "FLFHMCLauncherApp.exe")
$quickstart = Require-File (Join-Path $BuildDir "FLFQuickStart.exe")
$spdlog = Require-File (Join-Path $BuildDir "spdlog.dll")

# 找 VS 自带的 x64 VC runtime；引导器自身用 /MT，但 GUI 仍是 /MD。
$crtFiles = @()
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vs = (& $vswhere -latest -products * -property installationPath | Select-Object -First 1)
    if ($vs) {
        $redist = Join-Path $vs "VC\Redist\MSVC"
        if (Test-Path $redist) {
            $crtDir = Get-ChildItem $redist -Directory |
                Where-Object { $_.Name -match '^\d+(\.\d+)+$' -and (Test-Path (Join-Path $_.FullName "x64")) } |
                Sort-Object { [version]$_.Name } -Descending |
                Select-Object -First 1
            if ($crtDir) {
                $crtFiles = @(Get-ChildItem (Join-Path $crtDir.FullName "x64") -Recurse -File |
                    Where-Object { $_.Name -match '^(msvcp140|vcruntime140).*\.dll$' } |
                    Sort-Object Name -Unique)
            }
        }
    }
}
if ($crtFiles.Count -eq 0) {
    throw "找不到 VC++ x64 runtime DLL；请安装 Visual Studio C++ 工具链"
}

$files = [ordered]@{
    "FLFHMCLauncherApp.exe" = $app
    "FLFQuickStart.exe"     = $quickstart
    "spdlog.dll"            = $spdlog
}
$appManifest = Join-Path $BuildDir "FLFHMCLauncherApp.exe.manifest"
if (Test-Path $appManifest -PathType Leaf) {
    $files["FLFHMCLauncherApp.exe.manifest"] = (Resolve-Path $appManifest).Path
}
foreach ($dll in $crtFiles) {
    if (-not $files.Contains($dll.Name)) { $files[$dll.Name] = $dll.FullName }
}

$bootstrapBytes = [IO.File]::ReadAllBytes($bootstrap)
$payload = New-Object IO.MemoryStream
$writer = New-Object IO.BinaryWriter($payload)
$utf8 = New-Object Text.UTF8Encoding($false)
$writer.Write($utf8.GetBytes("FLFPAY01"))
$writer.Write([uint32]1)
$writer.Write([uint32]$files.Count)
foreach ($pair in $files.GetEnumerator()) {
    $name = $utf8.GetBytes([string]$pair.Key)
    $bytes = [IO.File]::ReadAllBytes([string]$pair.Value)
    $writer.Write([uint32]$name.Length)
    $writer.Write($name)
    $writer.Write([uint64]$bytes.Length)
    $writer.Write($bytes)
}
$writer.Flush()

$output = New-Object IO.MemoryStream
$output.Write($bootstrapBytes, 0, $bootstrapBytes.Length)
$offset = [uint64]$output.Length
$output.Write($payload.ToArray(), 0, [int]$payload.Length)
$footer = New-Object IO.MemoryStream
$footerWriter = New-Object IO.BinaryWriter($footer)
$footerWriter.Write($utf8.GetBytes("FLFEND01"))
$footerWriter.Write($offset)
$footerWriter.Write([uint64]$payload.Length)
$footerWriter.Flush()
$output.Write($footer.ToArray(), 0, [int]$footer.Length)

$temp = "$OutputExe.tmp"
[IO.File]::WriteAllBytes($temp, $output.ToArray())
Move-Item -Force $temp $OutputExe
$size = (Get-Item $OutputExe).Length
Write-Host ("packed {0} files -> {1} ({2} bytes)" -f $files.Count, $OutputExe, $size)
