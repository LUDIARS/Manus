# 本社 (Windows 実機) で C++ 版 Manus を確かめる手順。クラウドでは実行していない。
#
#   pwsh -File native/tests/tools/honsha-check.ps1            # ビルド + ctest + golden 再記録
#   pwsh -File native/tests/tools/honsha-check.ps1 -Smoke     # + 実行ファイルの起動確認 (入力は注入しない)
#
# 前提: Visual Studio 2022 (C++ デスクトップ)、CMake 3.24+、OpenSSL 3 (find_package で見つかる場所、
#       例: -DOPENSSL_ROOT_DIR=C:\Program Files\OpenSSL-Win64)、Node.js 22 (golden 記録のみ)。
# 実行場所: Manus リポのルート。
param(
    [switch]$Smoke,
    [string]$OpenSslRoot = "",
    [int]$SmokePort = 55390
)
$ErrorActionPreference = "Stop"
$root = (Get-Location).Path
$build = Join-Path $root "native/build-win"

Write-Host "== 1. configure / build (MSVC, Release)"
$configure = @("-S", "native", "-B", $build, "-A", "x64", "-DMANUS_WARNINGS_AS_ERRORS=ON")
if ($OpenSslRoot -ne "") { $configure += "-DOPENSSL_ROOT_DIR=$OpenSslRoot" }
cmake @configure
if ($LASTEXITCODE -ne 0) { throw "configure failed" }
cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed (warnings are errors)" }

Write-Host "== 2. golden を Node 実装から記録し直す"
npm ci --ignore-scripts --no-audit --no-fund
if ($LASTEXITCODE -ne 0) { throw "npm ci failed" }
npx tsx native/tests/tools/record-golden.ts
if ($LASTEXITCODE -ne 0) { throw "golden recording failed" }

Write-Host "== 3. ctest (win32_input_sink は SendInput を偽の関数に差し替えて実行)"
ctest --test-dir $build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "ctest failed" }

if ($Smoke) {
    Write-Host "== 4. 起動確認 (一時ディレクトリ・ポート $SmokePort・入力は注入しない)"
    $exe = Join-Path $build "bin/Release/manus.exe"
    if (-not (Test-Path $exe)) { $exe = Join-Path $build "bin/manus.exe" }
    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("manus-smoke-" + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $tmp | Out-Null
    Copy-Item -Recurse (Join-Path $root "native/tests/fixtures/http-root/public") (Join-Path $tmp "public")
    & $exe set-pin 482913 --root $tmp
    if ($LASTEXITCODE -ne 0) { throw "set-pin failed" }
    $env:MANUS_PORT = "$SmokePort"
    $proc = Start-Process -FilePath $exe -ArgumentList @("serve", "--root", $tmp) -PassThru -NoNewWindow
    try {
        Start-Sleep -Seconds 2
        $health = Invoke-RestMethod -Uri "http://127.0.0.1:$SmokePort/health"
        if ($health.helper -ne "up") { throw "/health の helper が up ではない" }
        $pair = Invoke-RestMethod -Method Post -Uri "http://127.0.0.1:$SmokePort/api/pair" -ContentType "application/json" -Body '{"pin":"482913"}'
        if (-not ($pair.token -match '^\d+\.[0-9a-f]{64}$')) { throw "token の形が違う" }
        $page = Invoke-WebRequest -Uri "http://127.0.0.1:$SmokePort/" -UseBasicParsing
        if ($page.StatusCode -ne 200) { throw "静的配信が 200 ではない" }
        Write-Host "smoke OK"
    } finally {
        Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue
        Remove-Item Env:MANUS_PORT
        Remove-Item -Recurse -Force $tmp
    }
}
Write-Host "== done"
