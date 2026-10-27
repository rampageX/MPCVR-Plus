@echo off
setlocal
set "OSD_PREPARE_SELF=%~f0"
set "OSD_PREPARE_MODE=%~1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$p=[regex]::Match([IO.File]::ReadAllText($env:OSD_PREPARE_SELF),'(?ms)^# BEGIN_POWERSHELL\r?\n(.*)\z'); & ([scriptblock]::Create($p.Groups[1].Value))"
set "OSD_PREPARE_RESULT=%ERRORLEVEL%"
echo.
pause
exit /b %OSD_PREPARE_RESULT%
# BEGIN_POWERSHELL
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$releaseRoot = [IO.Path]::GetDirectoryName($env:OSD_PREPARE_SELF)
$manifestPath = Join-Path $releaseRoot 'OSD-RELEASE.json'
$notesPath = Join-Path $releaseRoot 'OSD-RELEASE.md'
$expectedRepo = 'rampageX/MPCVR-Plus'
$utf8 = New-Object System.Text.UTF8Encoding($false)
$dryRun = $env:OSD_PREPARE_MODE -eq '/dry-run'

function Read-OsdVersion([string]$text) {
    $m = [regex]::Match($text, '(?m)^\s*#define\s+OSD_PLUS_VERSION_W\s+L"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"\s*$')
    if (!$m.Success) { throw 'Missing or invalid OSD_PLUS_VERSION_W in Source/OsdPlus/Version.h.' }
    return ($m.Groups[1].Value + '.' + $m.Groups[2].Value + '.' + $m.Groups[3].Value)
}

function Set-BuildVersion {
    $git = Get-Command git -ErrorAction Stop
    $branch = & $git.Source -C $releaseRoot branch --show-current
    if ($LASTEXITCODE -ne 0 -or $branch -ne 'osd-plus') { throw 'Switch to osd-plus first.' }
    $unmerged = & $git.Source -C $releaseRoot diff --name-only --diff-filter=U
    if ($LASTEXITCODE -ne 0 -or $unmerged) { throw 'Finish resolving the merge first.' }
    $version = (Read-Host 'OSD version to build (example: 1.0.5)').Trim()
    $header = "#pragma once`r`n`r`n// Set before building the accepted OSD Plus binary.`r`n#define OSD_PLUS_VERSION_W L`"$version`"`r`n"
    if ((Read-OsdVersion $header) -ne $version) { throw 'Use a three-part version such as 1.0.5.' }
    $helperPath = Join-Path $releaseRoot 'Source/Helper.cpp'
    $statsPath = Join-Path $releaseRoot 'Source/DX11VideoProcessorStats.cpp'
    $versionPath = Join-Path $releaseRoot 'Source/OsdPlus/Version.h'
    $helper = [IO.File]::ReadAllText($helperPath)
    $stats = [IO.File]::ReadAllText($statsPath)
    if ($helper -match '(?m)^[<=>]{7}' -or $stats -match '(?m)^[<=>]{7}') { throw 'Source contains merge conflict markers.' }
    $include = '#include "OsdPlus/Version.h"'
    foreach ($kind in @('helper','stats')) {
        $text = if ($kind -eq 'helper') { $helper } else { $stats }
        if (!$text.Contains($include)) {
            if (!$text.Contains('#include "stdafx.h"')) { throw "Cannot find include anchor in $kind." }
            $text = $text.Replace('#include "stdafx.h"', ('#include "stdafx.h"' + "`r`n" + $include))
        }
        if ($kind -eq 'helper') { $helper = $text } else { $stats = $text }
    }
    $append = 'version.append(L" | OSD-Plus " OSD_PLUS_VERSION_W);'
    if (!$helper.Contains($append)) {
        $anchor = '#ifdef _WIN64'
        if ([regex]::Matches($helper, [regex]::Escape($anchor)).Count -ne 1) { throw 'Helper.cpp changed: inspect version insertion manually.' }
        $helper = $helper.Replace($anchor, ("`t" + $append + "`r`n" + $anchor))
    }
    # The short OSD header can be constructed independently of GetVersionStr.
    # Do not duplicate our version if an upstream header uses GetVersionStr later.
    $statsAppend = 'if (str.find(L"OSD-Plus ") == std::wstring::npos) str.append(L" | OSD-Plus " OSD_PLUS_VERSION_W);'
    if (!$stats.Contains($statsAppend)) {
        $anchor = 'str.assign(m_strStatsHeader);'
        if ([regex]::Matches($stats, [regex]::Escape($anchor)).Count -ne 1) { throw 'Statistics layout changed: inspect version insertion manually.' }
        $stats = $stats.Replace($anchor, ($anchor + "`r`n`t" + $statsAppend))
    }
    Write-Host "Build version: OSD-Plus $version"
    Write-Host 'Targets: Source/Helper.cpp, Source/DX11VideoProcessorStats.cpp, Source/OsdPlus/Version.h'
    if ($dryRun) {
        Write-Host '[DRY RUN] No source files changed. Run /version to apply.'
        return
    }
    $paths = @($helperPath, $statsPath, $versionPath)
    $texts = @($helper, $stats, $header)
    $backup = Join-Path ([IO.Path]::GetTempPath()) ('MPCVR-Plus-Version-' + [guid]::NewGuid().ToString('N'))
    [IO.Directory]::CreateDirectory($backup) | Out-Null
    $old = @(); $existed = @()
    for ($i = 0; $i -lt $paths.Count; $i++) {
        $exists = Test-Path -LiteralPath $paths[$i]
        $existed += $exists
        $bytes = if ($exists) { [IO.File]::ReadAllBytes($paths[$i]) } else { $null }
        $old += ,$bytes
        if ($exists) { [IO.File]::WriteAllBytes((Join-Path $backup ([IO.Path]::GetFileName($paths[$i]))), $bytes) }
    }
    try {
        for ($i = 0; $i -lt $paths.Count; $i++) {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($paths[$i])) | Out-Null
            $encoding = if ($existed[$i] -and $old[$i].Length -ge 3 -and $old[$i][0] -eq 239 -and $old[$i][1] -eq 187 -and $old[$i][2] -eq 191) { New-Object System.Text.UTF8Encoding($true) } else { $utf8 }
            [IO.File]::WriteAllText($paths[$i], $texts[$i], $encoding)
        }
    } catch {
        for ($i = 0; $i -lt $paths.Count; $i++) {
            if ($existed[$i]) { [IO.File]::WriteAllBytes($paths[$i], $old[$i]) }
            elseif (Test-Path -LiteralPath $paths[$i]) { Remove-Item -LiteralPath $paths[$i] }
        }
        throw
    }
    Write-Host "Version prepared. Backups: $backup" -ForegroundColor Green
    Write-Host 'Commit and Push, wait for OSD Plus build, then test its binary.'
    Write-Host 'After acceptance, run /write with that build URL. Do not change the version after building.'
}

function Get-GitHubJson([string]$path) {
    $headers = @{
        Accept = 'application/vnd.github+json'
        'User-Agent' = 'MPCVR-Plus-Release-Helper'
        'X-GitHub-Api-Version' = '2022-11-28'
    }
    if ($env:GH_TOKEN) { $headers.Authorization = 'Bearer ' + $env:GH_TOKEN }
    try {
        Invoke-RestMethod -Uri ('https://api.github.com/repos/' + $expectedRepo + '/' + $path) -Headers $headers -TimeoutSec 30
    } catch {
        if ($_.ErrorDetails.Message -match 'rate limit exceeded') {
            throw 'GitHub public API rate limit reached. Retry later; no files have been changed.'
        }
        throw
    }
}

function Prepare-Release([string]$runUrl) {
    if (!(Test-Path -LiteralPath $manifestPath) -or !(Test-Path -LiteralPath $notesPath) -or
        !(Test-Path -LiteralPath (Join-Path $releaseRoot '.github/workflows/osd-plus-release.yml'))) {
        throw 'Place this CMD in the MPCVR-Plus repository root, beside OSD-RELEASE.json.'
    }
    $headPath = Join-Path $releaseRoot '.git/HEAD'
    if (Test-Path -LiteralPath $headPath) {
        if ([IO.File]::ReadAllText($headPath).Trim() -ne 'ref: refs/heads/osd-plus') {
            throw 'Switch to the osd-plus branch in GitHub Desktop first.'
        }
    }
    $uri = $null
    if (![uri]::TryCreate($runUrl.Trim().Trim('"'), [UriKind]::Absolute, [ref]$uri) -or
        $uri.Scheme -ne 'https' -or $uri.Host -ne 'github.com' -or $uri.UserInfo) {
        throw 'Paste an HTTPS github.com Actions run URL.'
    }
    $match = [regex]::Match($uri.AbsolutePath, '^/([^/]+/[^/]+)/actions/runs/([0-9]+)(?:/.*)?/?$')
    if (!$match.Success -or $match.Groups[1].Value -ine $expectedRepo) {
        throw "Expected https://github.com/$expectedRepo/actions/runs/NUMBER"
    }
    $runId = [long]$match.Groups[2].Value
    $current = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
    Write-Host 'Checking the accepted build (read-only GitHub API)...'
    $run = Get-GitHubJson "actions/runs/$runId"
    if ($run.name -ne 'OSD Plus build' -or $run.head_branch -ne 'osd-plus' -or
        $run.status -ne 'completed' -or $run.conclusion -ne 'success') {
        throw 'Choose a successful OSD Plus build on osd-plus, not Build VideoRenderer.'
    }
    $sha = [string]$run.head_sha
    if ($sha -notmatch '^[0-9a-f]{40}$') { throw 'The build has an invalid source SHA.' }
    $artifactName = 'MPCVR-Plus-OSD-' + $sha
    $artifactResponse = Get-GitHubJson "actions/runs/$runId/artifacts?per_page=100"
    $artifacts = @($artifactResponse.artifacts | Where-Object { $_.name -eq $artifactName -and !$_.expired })
    if ($artifacts.Count -ne 1) { throw 'The matching build artifact is missing or expired.' }

    # Reject a different build's local source before changing release files.
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) {
        $production = @('Source','Include','Shaders','external','NgxShim','common.props',
            'Directory.Build.props','platform.props','MpcVideoRenderer.sln','build_mpcvr.cmd','update_revision.cmd')
        $dirty = & $git.Source -C $releaseRoot status --porcelain -- @production
        if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect the local repository.' }
        if ($dirty) { throw 'Commit and build your production-source changes before preparing a release.' }
        & $git.Source -C $releaseRoot diff --quiet $sha HEAD -- @production
        if ($LASTEXITCODE -ne 0) { throw 'Local production source differs from this build, or its commit is missing locally. Fetch origin and select the tested build.' }
    }

    $published = @()
    $page = 1
    do {
        $response = Get-GitHubJson "releases?per_page=100&page=$page"
        $batch = @($response)
        foreach ($item in $batch) {
            if (!$item.draft -and !$item.prerelease -and $item.tag_name -match '^osd-v([0-9]+\.[0-9]+\.[0-9]+)$') {
                $published += [pscustomobject]@{ Version = [version]$Matches[1]; Tag = [string]$item.tag_name }
            }
        }
        $page++
    } while ($batch.Count -eq 100)
    if ($current.source_commit -eq $sha -and @($published | Where-Object { $_.Tag -eq $current.tag }).Count -gt 0) {
        Write-Host "This source is already configured for published release $($current.tag). No files changed."
        if ($dryRun) {
            Write-Host '[DRY RUN] Existing published configuration (no new release needed):' -ForegroundColor Cyan
            Write-Host ([IO.File]::ReadAllText($manifestPath))
            Write-Host ([IO.File]::ReadAllText($notesPath))
        }
        return
    }
    $latest = $published | Sort-Object Version -Descending | Select-Object -First 1
    # Read the version from the exact accepted commit, never from today's header
    # and never bump the tag after its binary has already been built.
    if (!$git) { throw 'Git is required to verify the version embedded in the accepted source.' }
    $builtHeader = & $git.Source -C $releaseRoot show ($sha + ':Source/OsdPlus/Version.h') 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'This build has no OSD version header. Run /version, commit, build and test first.' }
    $version = Read-OsdVersion ($builtHeader -join "`n")
    $builtHelper = & $git.Source -C $releaseRoot show ($sha + ':Source/Helper.cpp')
    if ($LASTEXITCODE -ne 0 -or ($builtHelper -join "`n") -notmatch 'version\.append\(L" \| OSD-Plus " OSD_PLUS_VERSION_W\)') {
        throw 'The accepted build does not append the OSD version. Run /version and rebuild.'
    }
    if ($latest -and [version]$version -le $latest.Version) { throw "OSD $version is not newer than published $($latest.Tag). Set a new version BEFORE building." }
    $tag = 'osd-v' + $version
    $changes = @()
    if ($latest) {
        $comparison = Get-GitHubJson "compare/$($latest.Tag)...$sha"
        foreach ($commit in $comparison.commits) {
            $subject = ([string]$commit.commit.message -split '\r?\n', 2)[0].Trim()
            if ($subject -match '^[\x20-\x7e]+$' -and $subject -notmatch '^(Release |Publish |Tag |Merge )') {
                $changes += '- ' + $subject.Replace('<','&lt;').Replace('>','&gt;')
            }
        }
    }
    if (!$changes.Count) { $changes = @('- Update the OSD customization using the accepted build specified below.') }
    $changes = @($changes | Select-Object -Unique)
    $manifest = [ordered]@{
        tag = $tag
        publication_revision = 2
        title = "MPCVR-Plus OSD v$version"
        source_commit = $sha
        build_run = $runId
        artifact_name = $artifactName
        asset_name = "MPCVR-Plus-OSD-v$version.zip"
    }
    $notes = @(
        "# MPCVR-Plus OSD v$version", '', '## Changes', '', ($changes -join "`r`n"), '',
        '## Accepted build', '',
        "- Source commit: ``$sha``", "- Build: $($run.html_url)",
        '- Publication reuses this successful build without rebuilding the filters.',
        '- Confirm playback acceptance before committing these release files.', '',
        '## Package', '',
        'Includes Win32 and x64 filters, English documentation, build provenance',
        'and a SHA-256 package checksum. Existing functionality and limitations',
        'remain as documented in OSD-PLUS.md.', ''
    ) -join "`r`n"
    $json = ($manifest | ConvertTo-Json -Depth 4) + "`r`n"
    if ($dryRun) {
        Write-Host "[DRY RUN] Would prepare $tag from build $runId" -ForegroundColor Cyan
        Write-Host '--- OSD-RELEASE.json ---'
        Write-Host $json
        Write-Host '--- OSD-RELEASE.md ---'
        Write-Host $notes
        Write-Host 'DRY RUN complete: no release files, backups, commits, tags or releases were created or changed.'
        return
    }
    $stamp = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
    $backup = Join-Path ([IO.Path]::GetTempPath()) ('MPCVR-Plus-Release-Backups/' + $stamp)
    [IO.Directory]::CreateDirectory($backup) | Out-Null
    $oldJson = [IO.File]::ReadAllBytes($manifestPath)
    $oldNotes = [IO.File]::ReadAllBytes($notesPath)
    [IO.File]::WriteAllBytes((Join-Path $backup 'OSD-RELEASE.json'), $oldJson)
    [IO.File]::WriteAllBytes((Join-Path $backup 'OSD-RELEASE.md'), $oldNotes)
    try {
        [IO.File]::WriteAllText($manifestPath, $json, $utf8)
        [IO.File]::WriteAllText($notesPath, $notes, $utf8)
    } catch {
        [IO.File]::WriteAllBytes($manifestPath, $oldJson)
        [IO.File]::WriteAllBytes($notesPath, $oldNotes)
        throw
    }
    Write-Host "Prepared $tag from build $runId" -ForegroundColor Green
    Write-Host "Source: $sha"
    Write-Host 'Updated: OSD-RELEASE.json and OSD-RELEASE.md'
    Write-Host "Backups: $backup"
    if (!$git) { Write-Host 'Git is not on PATH. The publication workflow will verify production-source matching.' }
    Write-Host ''
    Write-Host 'Review the English notes, then Commit and Push in GitHub Desktop.'
    Write-Host 'Watch the Publish accepted OSD release workflow.'
    Write-Host 'This helper does not commit, push, create tags or publish anything.'
}

try {
    $mode = $env:OSD_PREPARE_MODE
    if (!$mode) {
        Write-Host '1. Set version BEFORE building'
        Write-Host '2. Preview version changes (dry run)'
        Write-Host '3. Prepare release from an accepted build'
        Write-Host '4. Preview release files (dry run)'
        $choice = Read-Host 'Choose 1-4 (Enter to exit)'
        if (!$choice) { exit 0 }
        $mode = switch ($choice) { '1' { '/version' } '2' { '/version-dry-run' } '3' { '/write' } '4' { '/dry-run' } default { throw 'Choose 1-4.' } }
    }
    if ($mode -notin @('/dry-run','/write','/version','/version-dry-run')) {
        throw 'Usage: Prepare_OSD_Release.cmd [/version | /version-dry-run | /dry-run | /write]'
    }
    $dryRun = $mode -in @('/dry-run','/version-dry-run')
    if ($mode -in @('/version','/version-dry-run')) {
        Set-BuildVersion
        exit 0
    }
    Write-Host 'MPCVR-Plus - Prepare accepted OSD release'
    if ($dryRun) { Write-Host '[DRY RUN - preview only, no file changes]' -ForegroundColor Cyan }
    Write-Host ''
    $runUrl = Read-Host 'Paste the successful OSD Plus build page URL'
    if ([string]::IsNullOrWhiteSpace($runUrl)) { throw 'No URL supplied. No files changed.' }
    Prepare-Release $runUrl
    exit 0
} catch {
    Write-Host ''
    Write-Host ('ERROR: ' + $_.Exception.Message) -ForegroundColor Red
    Write-Host 'Preparation did not complete. Existing release files were preserved or restored.'
    exit 1
}
