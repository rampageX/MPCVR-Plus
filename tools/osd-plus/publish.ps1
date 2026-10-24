$ErrorActionPreference = 'Stop'
$release = Get-Content OSD-RELEASE.json -Raw | ConvertFrom-Json
$archives = @(Get-ChildItem accepted -Recurse -Filter 'MpcVideoRenderer*.zip')
if ($archives.Count -ne 1) { throw 'Expected exactly one upstream build package' }
New-Item -ItemType Directory -Path verified, release-package -Force | Out-Null
Expand-Archive -LiteralPath $archives[0].FullName -DestinationPath verified

$binaryHashes = @{}
foreach ($binary in @(@('MpcVideoRenderer.ax', 0x014c), @('MpcVideoRenderer64.ax', 0x8664))) {
    $filterFiles = @(Get-ChildItem verified -Recurse -Filter $binary[0])
    if ($filterFiles.Count -ne 1) { throw "Missing or duplicate filter: $($binary[0])" }
    $data = [IO.File]::ReadAllBytes($filterFiles[0].FullName)
    if ($data.Length -lt 64 -or $data[0] -ne 0x4d -or $data[1] -ne 0x5a) { throw 'Invalid filter image' }
    $pe = [BitConverter]::ToInt32($data, 0x3c)
    if ($pe -lt 0 -or $pe + 6 -gt $data.Length -or [BitConverter]::ToUInt32($data, $pe) -ne 0x4550 -or [BitConverter]::ToUInt16($data, $pe + 4) -ne $binary[1]) { throw 'Unexpected filter PE architecture' }
    $binaryHashes[$binary[0]] = (Get-FileHash -LiteralPath $filterFiles[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}

# Keep the accepted package intact, adding documentation and provenance only.
$asset = Join-Path (Resolve-Path release-package) $release.asset_name
Copy-Item -LiteralPath $archives[0].FullName -Destination $asset
@{
    source_commit = $release.source_commit
    build_run = $release.build_run
    build_url = "https://github.com/$env:GITHUB_REPOSITORY/actions/runs/$($release.build_run)"
    tag = $release.tag
    binary_sha256 = $binaryHashes
} | ConvertTo-Json -Depth 4 | Set-Content BUILD-INFO.json -Encoding utf8
$sevenZip = Join-Path ${env:ProgramFiles} '7-Zip/7z.exe'
& $sevenZip a -tzip $asset OSD-PLUS.md OSD-RELEASE.md BUILD-INFO.json
if ($LASTEXITCODE -ne 0) { throw 'Failed to add release documentation' }
& $sevenZip t $asset
if ($LASTEXITCODE -ne 0) { throw 'Release archive validation failed' }
$hash = (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant()
$checksum = Join-Path (Resolve-Path release-package) 'SHA256SUMS.txt'
"$hash  $($release.asset_name)" | Set-Content $checksum -Encoding ascii

# Never overwrite a previously published release or move an existing tag.
$existing = gh release list --repo $env:GITHUB_REPOSITORY --limit 100 --json tagName | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect existing releases' }
if ($existing.tagName -contains $release.tag) { throw 'Release already exists; refusing to replace it' }
$tags = gh api "repos/$env:GITHUB_REPOSITORY/git/matching-refs/tags/$($release.tag)" | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect tag refs' }
if ($tags | Where-Object { $_.ref -eq "refs/tags/$($release.tag)" }) { throw 'Tag already exists; refusing to move it' }
gh release create $release.tag $asset $checksum --repo $env:GITHUB_REPOSITORY --target $release.source_commit --title $release.title --notes-file OSD-RELEASE.md --latest
if ($LASTEXITCODE -ne 0) { throw 'Release publication failed' }
