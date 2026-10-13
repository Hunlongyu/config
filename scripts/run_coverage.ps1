param([string]$BuildDirectory = "build/windows-debug")
$ErrorActionPreference = "Stop"
if (-not (Get-Command OpenCppCoverage -ErrorAction SilentlyContinue)) {
    throw "请安装 OpenCppCoverage，并从初始化 MSVC 的终端运行。"
}
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $BuildDirectory))
if (-not (Test-Path -LiteralPath (Join-Path $buildPath "CMakeCache.txt"))) {
    throw "请先按 docs/BUILD.zh-CN.md 配置构建目录。"
}
cmake --build $buildPath --config Debug
if ($LASTEXITCODE -ne 0) { throw "构建失败。" }
Push-Location -LiteralPath $buildPath
try {
    OpenCppCoverage --sources "$projectRoot\include\config" --excluded_sources "$buildPath" --cover_children -- ctest -C Debug --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "测试或覆盖率采集失败。" }
} finally {
    Pop-Location
}
