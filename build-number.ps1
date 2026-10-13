# Takes DS5-USBIP's next build number and commits it, before build.ps1 compiles.
#
#   .\build-number.ps1           build.ps1 runs it: nobody else needs to
#   .\build-number.ps1 -Peek     the number the next build would take; changes nothing
#
# ============================================================================
# WHY, AND WHY THIS WAY
# ============================================================================
#
# rhoquinn8217, 2026-10-12: "we need a build number for DS5-USBIP like we have
# for aurora-tv". A version names a release, and many builds share one; the
# build number says which build is running. The program shows it as
# "0.1.0 (N)", the form aurora-tv shows its own in (include/ctm/product.h).
#
# It is taken the way aurora-tv's build script takes its own
# (repos/scripts/build-aurora-docker.ps1), for the reasons that script learnt
# the hard way:
#
#   1. THE NEXT NUMBER FREE, NOT THE NUMBER IN THE FILE: the highest in
#      include/ctm/build_number.h on any branch, local or remote, or in this
#      PC's record below, plus one. aurora-tv once read the number as it
#      stood, and twice gave one number to two different builds, one of them
#      the build a TV was running.
#   2. COMMITTED AS "Build N" BEFORE ANYTHING COMPILES. A number in no commit
#      is a claim about code nobody can find again. So a tree with anything
#      uncommitted in it is refused, and a build that then fails has used its
#      number up: fix it, commit, build again, and that build is N+1.
#   3. NEVER BY HAND. A committed number counts as used, so a hand bump makes
#      the next build step past it, and the number typed was never built.
#
# Two things aurora-tv's way does not need, and this does:
#
#   4. THE RECORD: build-numbers.txt in the repository's git folder, which
#      every worktree shares. One line a build, tab-separated: the number, the
#      time, the branch, the commit, the configuration and the folder.
#      aurora-tv keeps every package it ever built and counts those too; a
#      listener build overwrites the last one, so the record is what remembers
#      a number whose branch was deleted, and which commit a number was. It is
#      not in the repository and never leaves this PC.
#   5. ONE BUILD AT A TIME TAKES A NUMBER. Several sessions build here at
#      once. The record is held open, shared with nobody, from the search to
#      the commit, so two builds cannot take the same number: the second
#      waits.
#
# A MERGE where both sides took numbers conflicts on the one line in
# build_number.h. Keep either side: the next build takes the next number free,
# whatever that line says.

[CmdletBinding()]
param(
    [switch]$Peek,
    # Written into the record, so a number says what kind of build it was.
    [string]$Configuration = ''
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$NumberFile = 'include/ctm/build_number.h'
$NumberPath = [System.IO.Path]::Combine($Root, 'include', 'ctm', 'build_number.h')
$NumberLine = '^#define PRODUCT_BUILD ([0-9]+)'
$script:record = $null

# Stops here, says why, and lets the record go.
function Stop-Build([string]$why) {
    if ($script:record) { $script:record.Dispose(); $script:record = $null }
    Write-Host ('build-number: ' + $why) -ForegroundColor Red
    exit 1
}

# git, with its complaints thrown away rather than turned into a stop: Windows
# PowerShell 5.1 makes a native command's error output a terminating error
# under 'Stop', and "that branch has no such file" is an answer here, not a
# failure. $LASTEXITCODE says how it went.
function Invoke-Git {
    $saved = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & git -C $Root @args 2>$null } finally { $ErrorActionPreference = $saved }
}

if (-not (Test-Path -LiteralPath $NumberPath)) {
    Stop-Build "this tree has no $NumberFile, so it has no number to take. Nothing was built."
}

$gitDir = ((Invoke-Git rev-parse --path-format=absolute --git-common-dir) -join '').Trim()
if ($LASTEXITCODE -ne 0 -or -not $gitDir) { Stop-Build "$Root is not in a git repository. Nothing was built." }
$recordPath = Join-Path $gitDir 'build-numbers.txt'

# 5. Held, shared with nobody, until the number is committed and written down.
$giveUpAt = (Get-Date).AddMinutes(2)
while (-not $script:record) {
    try {
        $script:record = [System.IO.File]::Open($recordPath, [System.IO.FileMode]::OpenOrCreate,
                                                [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    } catch {
        $busy = ($_.Exception -is [System.IO.IOException]) -or ($_.Exception.InnerException -is [System.IO.IOException])
        if (-not $busy) { throw }
        if ((Get-Date) -gt $giveUpAt) { Stop-Build "another build has held $recordPath for two minutes. Nothing was built." }
        Start-Sleep -Milliseconds 250
    }
}

# 4. Every number this PC has handed out.
$max = 0
$reader = New-Object System.IO.StreamReader($script:record, (New-Object System.Text.UTF8Encoding($false)), $false, 4096, $true)
$recorded = $reader.ReadToEnd()
$reader.Dispose()
foreach ($l in ($recorded -split "`n")) {
    if ($l -match "^([0-9]+)`t") { if ([int]$Matches[1] -gt $max) { $max = [int]$Matches[1] } }
}

# 1. Every branch, local and remote: one git grep over all of them, and branch
# by branch only if that fails, so a failure can never read as "no numbers in
# use". (aurora-tv's script timed it: 0.1 s, where branch by branch took 8 s
# for 96 branches.)
$refs = @(Invoke-Git for-each-ref '--format=%(refname)' refs/heads refs/remotes | Where-Object { $_ -and $_ -notmatch '/HEAD$' })
$found = @()
if ($refs.Count -gt 0) {
    $found = @(Invoke-Git grep -h -E '^#define PRODUCT_BUILD [0-9]+' @refs '--' $NumberFile)
    if ($LASTEXITCODE -gt 1) {
        $found = @()
        foreach ($ref in $refs) { $found += @(Invoke-Git show "${ref}:$NumberFile") }
    }
}
# And this tree's own.
$found += @(Get-Content -LiteralPath $NumberPath)
foreach ($l in $found) {
    if ($l -match $NumberLine) { if ([int]$Matches[1] -gt $max) { $max = [int]$Matches[1] } }
}
$next = $max + 1

$dirty = @(Invoke-Git status --porcelain)
if ($LASTEXITCODE -ne 0) { Stop-Build "git status failed in $Root. Nothing was built." }

if ($Peek) {
    $state = if ($dirty.Count -gt 0) { 'has uncommitted changes, so a build would refuse' } else { 'is clean' }
    $script:record.Dispose(); $script:record = $null
    Write-Host "build-number: the next build is $next; this tree $state."
    exit 0
}

# 2. A number names a commit, so nothing may be left out of one.
if ($dirty.Count -gt 0) {
    $nl = [Environment]::NewLine
    $list = ($dirty | Select-Object -First 12 | ForEach-Object { '    ' + $_ }) -join $nl
    Stop-Build ('this tree has uncommitted changes, and a build number has to name a commit:' + $nl + $list + $nl +
                'Commit them, then build again. Nothing was built, and no number was taken.')
}

$text = [System.IO.File]::ReadAllText($NumberPath)
$taken = [regex]::Replace($text, '(?m)^#define PRODUCT_BUILD [0-9]+', "#define PRODUCT_BUILD $next")
if ($taken -ceq $text) { Stop-Build "found no PRODUCT_BUILD line to rewrite in $NumberFile. Nothing was built." }
[System.IO.File]::WriteAllText($NumberPath, $taken, (New-Object System.Text.UTF8Encoding($false)))
Invoke-Git commit -q -m "Build $next" -m 'Taken by build-number.ps1: the next number free across every branch and every build recorded on this PC.' -m 'Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>' '--' $NumberFile | Out-Null
if ($LASTEXITCODE -ne 0) {
    Invoke-Git checkout -q '--' $NumberFile | Out-Null
    Stop-Build "could not commit Build $next. Nothing was built."
}

# 4. Written down, then let go.
$branch = ((Invoke-Git branch --show-current) -join '').Trim()
if (-not $branch) { $branch = '(no branch)' }
$commit = ((Invoke-Git rev-parse --short=12 HEAD) -join '').Trim()
$entry = "$next`t$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`t$branch`t$commit`t$Configuration`t$Root`n"
$bytes = (New-Object System.Text.UTF8Encoding($false)).GetBytes($entry)
[void]$script:record.Seek(0, [System.IO.SeekOrigin]::End)
$script:record.Write($bytes, 0, $bytes.Length)
$script:record.Dispose(); $script:record = $null
Write-Host "build number: $next (committed as $commit on $branch)" -ForegroundColor Yellow
exit 0
