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
    $version = if ($latest) {
        '{0}.{1}.{2}' -f $latest.Version.Major, $latest.Version.Minor, ($latest.Version.Build + 1)
    } else { '1.0.0' }
    if ($current.tag -match '^osd-v([0-9]+\.[0-9]+\.[0-9]+)$') {
        $configured = [version]$Matches[1]
        if ($configured -gt [version]$version) { $version = $configured.ToString(3) }
    }
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
    if ($env:OSD_PREPARE_MODE -and $env:OSD_PREPARE_MODE -notin @('/dry-run','/write')) {
        throw 'Usage: Prepare_OSD_Release.cmd [/dry-run | /write]'
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
