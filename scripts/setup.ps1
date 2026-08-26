# setup.ps1 -- installs/verifies everything needed for a full fj dev
# environment on Windows: git, CMake, Visual Studio 2022 (Desktop C++
# workload) for the native windows-x64/windows-arm64 builds, and emsdk for
# the Web (Emscripten/WASM) build. Safe to re-run -- every step checks
# whether it's already done first, so this is also just a fast way to
# confirm the environment is still set up correctly.
#
# fjTests' test framework (Catch2) needs nothing here: CMake's
# FetchContent downloads/builds it automatically the first time you
# configure (see CMakeLists.txt) -- there's no separate install step for
# it.
#
# Windows-only -- Linux has no equivalent script; see README.md's Linux
# section for its one apt-get one-liner, simple enough not to need one.
# PowerShell 7 itself isn't installed by this script either (see
# README.md's Windows Prerequisites) -- it's what's running this script,
# so there's no way for the script to bootstrap it for itself.

#Requires -Version 7

if (-not $IsWindows) {
    Write-Error "setup.ps1 is Windows-only -- see README.md for Linux/Web prerequisites."
    exit 1
}

function Write-Step($msg) { Write-Host -ForegroundColor Green $msg }
function Write-Detail($msg) { Write-Host -ForegroundColor DarkGray "    $msg" }

function Install-IfMissing($command, $wingetId) {
    if (Get-Command $command -ErrorAction SilentlyContinue) {
        Write-Detail "$command already on PATH, skipping"
        return
    }
    Write-Detail "winget install --id $wingetId"
    winget install --id $wingetId -e --source winget --accept-package-agreements --accept-source-agreements
}

function Install-VisualStudioCppWorkload {
    # vswhere.exe ships with the VS Installer itself (present once any VS
    # instance has ever been installed) -- -requires asks it to only
    # return instances that actually have the "Desktop development with
    # C++" workload, not just any VS install.
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath
        if ($found) {
            Write-Detail "Visual Studio with the Desktop C++ workload already installed at $found, skipping"
            return
        }
    }
    # --override passes args straight to the VS Installer: --add asks for
    # the workload, whether or not VS itself is already present -- the VS
    # Installer is designed to be re-run this way to add a missing
    # workload to an existing install, so this is safe either way.
    Write-Detail "winget install --id Microsoft.VisualStudio.2022.Community (+ Desktop C++ workload)"
    winget install --id Microsoft.VisualStudio.2022.Community -e --source winget `
        --accept-package-agreements --accept-source-agreements `
        --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive --norestart"
}

# Persists a directory to the user's PATH (so it's there in future shells,
# not just this process) if it isn't already, and adds it to this
# process's own $env:PATH too so the rest of this script can use it
# immediately without needing a new shell.
function Add-PathEntry($dir) {
    $userPath = [Environment]::GetEnvironmentVariable('PATH', 'User')
    $entries = $userPath -split ';' | Where-Object { $_ -ne '' }
    if ($entries -notcontains $dir) {
        [Environment]::SetEnvironmentVariable('PATH', ($userPath.TrimEnd(';') + ";$dir"), 'User')
        Write-Detail "Added to PATH: $dir"
    }
    if (($env:PATH -split ';') -notcontains $dir) {
        $env:PATH += ";$dir"
    }
}

function Install-Emsdk {
    $emsdkDir = Join-Path $env:USERPROFILE "emsdk"
    $ninjaVersion = "1.13.2-64bit"
    $ninjaDirName = "1.13.2_64bit"

    if (-not (Test-Path $emsdkDir)) {
        Write-Detail "git clone https://github.com/emscripten-core/emsdk $emsdkDir"
        git clone https://github.com/emscripten-core/emsdk $emsdkDir
    }
    else {
        Write-Detail "emsdk already cloned at $emsdkDir, skipping clone"
    }

    # Windows Emscripten ships emcc.exe/emcc.py here, not an emcc.bat
    # shim (some older docs/tutorials reference one) -- .exe is the real
    # file to check for.
    $emccPath = Join-Path $emsdkDir "upstream\emscripten\emcc.exe"
    if (-not (Test-Path $emccPath)) {
        Write-Detail "emsdk install latest"
        & "$emsdkDir\emsdk.ps1" install latest
        Write-Detail "emsdk install ninja-$ninjaVersion"
        & "$emsdkDir\emsdk.ps1" install "ninja-$ninjaVersion"
    }
    else {
        Write-Detail "emcc already installed at $emccPath, skipping"
    }

    # You may see "error: tool or SDK not found" printed by the next two
    # lines -- a known, harmless quirk of emsdk.ps1's own activate command
    # on this machine (see PLAN.md's "Web (Emscripten) shell" section); it
    # still correctly sets EMSDK/EMSDK_NODE/EMSDK_PYTHON despite printing
    # it. --permanent didn't reliably add upstream\emscripten (where emcc
    # actually lives) to PATH though -- Add-PathEntry below fixes that
    # explicitly rather than trusting activate always gets it right.
    & "$emsdkDir\emsdk.ps1" activate latest --permanent | Out-Null
    & "$emsdkDir\emsdk.ps1" activate "ninja-$ninjaVersion" --permanent | Out-Null

    Add-PathEntry $emsdkDir
    Add-PathEntry (Join-Path $emsdkDir "upstream\emscripten")
    Add-PathEntry (Join-Path $emsdkDir "ninja\$ninjaDirName")

    [Environment]::SetEnvironmentVariable('EMSDK', $emsdkDir, 'User')
    $env:EMSDK = $emsdkDir
}

Write-Step "Checking native build prerequisites..."
Install-IfMissing git Git.Git
Install-IfMissing cmake Kitware.CMake
Install-VisualStudioCppWorkload

Write-Step "Setting up the Web (Emscripten) toolchain..."
Install-Emsdk

Write-Step "Done. Open a new shell so PATH changes take effect, then see README.md to build."
