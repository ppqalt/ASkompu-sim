param([Parameter(Mandatory = $true)][string]$Archive, [string]$CoreUpstream)
$ErrorActionPreference = 'Stop'
$archivePath = (Resolve-Path $Archive).Path
$previousVideoDriver = $env:SDL_VIDEODRIVER
$root = Join-Path ([System.IO.Path]::GetTempPath()) ('ASkompu ää välilyönti ' + [guid]::NewGuid())
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    Expand-Archive -LiteralPath $archivePath -DestinationPath $root
    $app = Join-Path $root 'ASkompu-simulaattori'
    $exe = Join-Path $app 'askompu-simulaattori.exe'
    $expected = @('askompu-simulaattori.exe', 'README.txt', 'BUILDINFO.txt', 'simulator-source.zip', 'tools/core_versions.py',
        'LISENSSIT/ASkompu-LICENSE.txt', 'LISENSSIT/SDL-LICENSE.txt',
        'LISENSSIT/ImGui-LICENSE.txt', 'LISENSSIT/Roboto-LICENSE.txt', 'LISENSSIT/THIRD_PARTY.md')
    $actual = @(Get-ChildItem -LiteralPath $root -Recurse -File | ForEach-Object {
        [System.IO.Path]::GetRelativePath($app, $_.FullName).Replace('\', '/')
    })
    if (Compare-Object $expected $actual) { throw 'Julkaisupaketin tiedostoluettelo ei vastaa odotettua.' }
    $working = Join-Path $root 'Erillinen työhakemisto'
    New-Item -ItemType Directory -Path $working | Out-Null
    $captures = Join-Path $root 'Tarkistuskuvat ää'
    New-Item -ItemType Directory -Path $captures | Out-Null
    $modes = @('--versio', '--tarkista')
    if ($CoreUpstream) { $modes += '--tarkista-ytimenvalitsin' }
    foreach ($mode in $modes) {
        $stdout = Join-Path $root 'tulos.txt'
        $stderr = Join-Path $root 'virhe.txt'
        $env:SDL_VIDEODRIVER = 'dummy'
        $arguments = @($mode)
        if ($mode -eq '--tarkista') { $arguments += @('--tarkistuskuvat', ('"' + $captures + '"')) }
        if ($mode -eq '--tarkista-ytimenvalitsin') {
            $arguments += @('--core-home', ('"' + (Join-Path $root 'Ytimen työtila') + '"'),
                '--core-upstream', ('"' + $CoreUpstream + '"'))
        }
        $process = Start-Process -FilePath $exe -WorkingDirectory $working -ArgumentList $arguments `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
        if (-not $process.WaitForExit(120000)) {
            $process.Kill()
            throw 'Paketin ajotarkistus ylitti aikarajan.'
        }
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) {
            Write-Output (Get-Content $stdout -Raw -Encoding utf8)
            Get-ChildItem -LiteralPath $root -Recurse -File |
                Where-Object { $_.Name -in @('state.json', 'backend.log', 'operation.log', 'requirements.tsv') } |
                ForEach-Object { Write-Output $_.FullName; Get-Content -LiteralPath $_.FullName -Raw -Encoding utf8 }
            throw "Paketin ajotarkistus epäonnistui: $(Get-Content $stderr -Raw)"
        }
        $result = Get-Content $stdout -Raw -Encoding utf8
        if ($mode -eq '--versio' -and $result -notmatch 'ASkompu-simulaattori 1\.0\.0') { throw 'Versiotuloste puuttuu.' }
        if ($mode -eq '--tarkista' -and $result -notmatch 'pienen ikkunan vieritys') { throw 'GUI-tarkistus ei valmistunut.' }
        if ($mode -eq '--tarkista-ytimenvalitsin' -and $result -notmatch 'pieni ikkuna ja virhetila') { throw 'Versionvalitsimen GUI-tarkistus ei valmistunut.' }
        Write-Output $result
    }
    if (@(Get-ChildItem -LiteralPath $captures -Filter '*.bmp').Count -ne 3) { throw 'UTF-8-polkuun tallennetut tarkistuskuvat puuttuvat.' }
    Write-Output 'Hyväksytty: paketin rakenne ja erillinen ajohakemisto ääkkösillä. SDL:n dummy-ajuri; ei Windows 11 -työpöytävarmennus.'
} finally {
    if ($null -eq $previousVideoDriver) { Remove-Item Env:SDL_VIDEODRIVER -ErrorAction SilentlyContinue }
    else { $env:SDL_VIDEODRIVER = $previousVideoDriver }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
