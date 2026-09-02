# deploy-web.ps1 -- builds the Release web (Emscripten/WASM) target and
# publishes it to GitHub Pages. Safe to re-run any time: this is the one
# command that updates the live demo.
#
# How it works: GitHub Pages serves static files from a branch. Rather
# than switching this repo's own working tree back and forth, a second,
# permanent git *worktree* lives alongside this repo (not inside it),
# checked out to an orphan `gh-pages` branch that holds nothing but the
# three built artifacts (as index.html/fj.js/fj.wasm) -- no source code,
# no history shared with main. First run creates that branch/worktree
# from scratch; every run after that just rebuilds, re-copies, and
# re-pushes.
#
# Custom domain (fj.davidlenihan.com) is opt-in, not automatic: a CNAME
# file in the published branch is what tells GitHub Pages to serve (and
# redirect the default URL to) that domain -- adding one before DNS is
# actually configured would break the default URL for anyone visiting
# it. So this script only *maintains* a CNAME file if one is already
# there; it never creates the first one. Until you're ready for the
# custom domain, the demo is reachable at the default
# https://lenihan.github.io/fj/ as soon as GitHub Pages is turned on
# (below) -- no DNS needed at all. When you *are* ready: create
# `$worktreePath\CNAME` containing `fj.davidlenihan.com` once (by hand,
# or ask Claude), then every future run of this script keeps it current.
#
# One-time setup this script does NOT do for you (see PLAN.md):
# - GitHub: repo Settings -> Pages -> Source "Deploy from a branch",
#   branch gh-pages / (root). (Custom domain field: leave blank for now
#   -- see above.)
# - If/when you add the custom domain: your DNS provider needs a CNAME
#   record, host "fj", value "lenihan.github.io".
#
# Windows-only (PowerShell), matching setup.ps1 -- a personal,
# on-demand deploy command run from a dev machine, not CI.

#Requires -Version 7

$ErrorActionPreference = "Stop"

function Write-Step($msg) { Write-Host -ForegroundColor Green $msg }
function Write-Detail($msg) { Write-Host -ForegroundColor DarkGray "    $msg" }

$repoRoot = Split-Path -Parent $PSScriptRoot
$worktreePath = Join-Path (Split-Path -Parent $repoRoot) "fj-gh-pages-worktree"
$defaultUrl = "https://lenihan.github.io/fj/"

Write-Step "Building web-release..."
Push-Location $repoRoot
try {
    cmake --workflow --preset web-release
    if ($LASTEXITCODE -ne 0) { throw "cmake --workflow --preset web-release failed (exit $LASTEXITCODE)" }
}
finally {
    Pop-Location
}

if (-not (Test-Path $worktreePath)) {
    Write-Step "First run: setting up the gh-pages worktree at $worktreePath..."
    Push-Location $repoRoot
    try {
        # If a gh-pages branch already exists on the remote (e.g. from a
        # prior manual attempt), reuse it instead of creating a second
        # orphan history -- ls-remote prints nothing (not an error) when
        # the branch doesn't exist, which is the common first-run case.
        $remoteBranch = git ls-remote --heads origin gh-pages
        if ($remoteBranch) {
            Write-Detail "origin/gh-pages already exists, checking it out"
            git worktree add $worktreePath gh-pages
            if ($LASTEXITCODE -ne 0) { throw "git worktree add failed (exit $LASTEXITCODE)" }
        }
        else {
            Write-Detail "No origin/gh-pages yet -- creating a fresh orphan branch"
            git worktree add --detach $worktreePath
            if ($LASTEXITCODE -ne 0) { throw "git worktree add --detach failed (exit $LASTEXITCODE)" }
            Push-Location $worktreePath
            try {
                git checkout --orphan gh-pages
                if ($LASTEXITCODE -ne 0) { throw "git checkout --orphan gh-pages failed (exit $LASTEXITCODE)" }
                # A fresh orphan checkout still has main's own files sitting
                # in the working tree (just unstaged for this branch) --
                # clear them so gh-pages starts genuinely empty.
                Get-ChildItem -Force | Where-Object { $_.Name -ne ".git" } | Remove-Item -Recurse -Force
            }
            finally {
                Pop-Location
            }
        }
    }
    finally {
        Pop-Location
    }
}

Write-Step "Copying build output into the gh-pages worktree..."
$buildDir = Join-Path $repoRoot "build\web\Release"
foreach ($file in @("fj.html", "fj.js", "fj.wasm")) {
    if (-not (Test-Path (Join-Path $buildDir $file))) {
        throw "Expected build output missing: $buildDir\$file -- did the web-release build actually succeed?"
    }
}
# fj.html -> index.html: GitHub Pages' default document, so both the
# default lenihan.github.io/fj/ URL and (once opted into, see the header
# comment) the custom domain work with no filename in the URL.
Copy-Item (Join-Path $buildDir "fj.html") (Join-Path $worktreePath "index.html") -Force
Copy-Item (Join-Path $buildDir "fj.js") (Join-Path $worktreePath "fj.js") -Force
Copy-Item (Join-Path $buildDir "fj.wasm") (Join-Path $worktreePath "fj.wasm") -Force
# Only *maintained*, never *created*, here -- see the header comment on
# why this script never writes a CNAME file that doesn't already exist.
$cnamePath = Join-Path $worktreePath "CNAME"
$customDomain = $null
if (Test-Path $cnamePath) {
    $customDomain = (Get-Content $cnamePath -Raw).Trim()
    Set-Content -Path $cnamePath -Value $customDomain
}

Write-Step "Publishing..."
Push-Location $worktreePath
try {
    git add -A
    $pending = git status --porcelain
    if (-not $pending) {
        Write-Detail "No changes since the last deploy -- nothing to push"
    }
    else {
        git commit -m "Deploy web build" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "git commit failed (exit $LASTEXITCODE)" }
        # force-with-lease, not a plain push: this branch's history is
        # disposable build output that nothing else ever reads or builds
        # on, so overwriting it is safe -- lease still protects against a
        # push from a second machine landing between this script's fetch
        # and its push.
        git push --force-with-lease -u origin gh-pages
        if ($LASTEXITCODE -ne 0) { throw "git push failed (exit $LASTEXITCODE)" }
        $liveUrl = if ($customDomain) { "http://$customDomain" } else { $defaultUrl }
        Write-Step "Deployed. Live at $liveUrl (may take a minute to update)."
    }
}
finally {
    Pop-Location
}
