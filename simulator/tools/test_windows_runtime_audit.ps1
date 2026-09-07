param([Parameter(Mandatory = $true)][string]$AuditScript)
$ErrorActionPreference = 'Stop'
$audit = (Resolve-Path $AuditScript).Path
$root = Join-Path ([System.IO.Path]::GetTempPath()) ('ASkompu DLL audit ' + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path "$root/source", "$root/stage" | Out-Null
    @'
cmake_minimum_required(VERSION 3.25)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
project(RuntimeAuditFixtures LANGUAGES C)
add_executable(static_crt main.c)
add_executable(dynamic_crt main.c)
set_property(TARGET dynamic_crt PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_library(external_dependency SHARED dependency.c)
add_executable(external_dll external.c)
target_link_libraries(external_dll PRIVATE external_dependency)
'@ | Set-Content -LiteralPath "$root/source/CMakeLists.txt" -Encoding utf8
    '#include <stdio.h>', 'int main(void) { return puts("runtime audit")==EOF; }' |
        Set-Content -LiteralPath "$root/source/main.c" -Encoding utf8
    '__declspec(dllexport) int dependency(void) { return 0; }' |
        Set-Content -LiteralPath "$root/source/dependency.c" -Encoding utf8
    '__declspec(dllimport) int dependency(void); int main(void) { return dependency(); }' |
        Set-Content -LiteralPath "$root/source/external.c" -Encoding utf8
    cmake -S "$root/source" -B "$root/build" -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'DLL-testiohjelmien määritys epäonnistui.' }
    cmake --build "$root/build" --config Release
    if ($LASTEXITCODE -ne 0) { throw 'DLL-testiohjelmien rakennus epäonnistui.' }
    foreach ($case in @('static_crt', 'dynamic_crt', 'external_dll', 'missing_dll')) {
        Get-ChildItem -LiteralPath "$root/stage" -File | Remove-Item
        $target = if ($case -eq 'missing_dll') { 'external_dll' } else { $case }
        Copy-Item -LiteralPath "$root/build/Release/$target.exe" -Destination "$root/stage/askompu-simulaattori.exe"
        if ($case -eq 'external_dll') {
            Copy-Item -LiteralPath "$root/build/Release/external_dependency.dll" -Destination "$root/stage"
        }
        $output = & cmake '-DCMAKE_INSTALL_CONFIG_NAME=Release' "-DCMAKE_INSTALL_PREFIX=$root/stage" -P $audit 2>&1
        $code = $LASTEXITCODE
        Write-Output $output
        if ($case -eq 'static_crt') {
            if ($code -ne 0) { throw 'Staattinen DLL-testiohjelma hylättiin.' }
        } else {
            if ($code -eq 0) { throw "DLL-auditointi hyväksyi virheellisen tapauksen: $case" }
            $reason = if ($case -eq 'dynamic_crt') { '(?i)vcruntime|msvcp' } else { 'external_dependency' }
            if (($output -join "`n") -notmatch $reason) { throw "DLL-hylkäyksen syy ei vastannut tapausta: $case" }
        }
        Write-Output "Hyväksytty DLL-auditin testi: $case"
    }
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
