# Setup prerequisites, build environment, dependencies

#Requires -Version 7

param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('x86', 'x64', 'arm', 'arm64')]
    [string]$TargetArch = $null
)
# Inputs
$QT_TAG = "v6.8.3" # LTS - Long-Term Support
if ($IsWindows) {
    # $VS_ENV_SCRIPT = Get-ChildItem -Recurse "$env:ProgramFiles\Microsoft Visual Studio\2022\*\Common7\Tools\Launch-VsDevShell.ps1"
    $VS_ENV_SCRIPT = Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\vcvarsall.bat" -Recurse
}


# Globals
$ROOT_DIR = Resolve-Path $PSScriptRoot/..
$REPO_DIR = Resolve-Path $ROOT_DIR/..
$PROCESSOR = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture

# What vcvarsall.bat uses
switch ($PROCESSOR) {
    "X64"   {$HOST_ARCH = 'x64'}
    "Arm64" {$HOST_ARCH = 'arm64'}  # ~20 min
    "Arm"   {$HOST_ARCH = 'arm'}
    "X86"   {$HOST_ARCH = 'x86'}
}
if (!$TargetArch) {$TARGET_ARCH = $HOST_ARCH}
else {$TARGET_ARCH = $TargetArch}

if ($HOST_ARCH -eq $TARGET_ARCH) {$HOST_TARGET = $HOST_ARCH}
else {$HOST_TARGET = $HOST_ARCH + "_" + $TARGET_ARCH}

function echo_command($cmd) {
    Write-Host -ForegroundColor Cyan $cmd 
    Invoke-Expression $cmd
}

function check_prerequisites {
    Write-Host -ForegroundColor Green "Checking prerequisites..."

    if ($IsWindows) {
        $required = "git", "cmake", $VS_ENV_SCRIPT
        $all_found = $true
        foreach($r in $required) {
            $found = Get-Command $r -ErrorAction SilentlyContinue
            if (!$found) {
                $all_found = $false
                Write-Host -ForegroundColor Red "    Could not find: $r"
            }
        }
        if ($all_found) {
            Write-Host -ForegroundColor Green "    Prerequisites: GOOD"
        }
        else {
            Write-Host -ForegroundColor Red "    ERROR: Cannot continue without prerequisites."
            exit    
        }
    }
}

function setup_build_environment {
    Write-Host -ForegroundColor Green "Setup build environment..." 
    if ($IsWindows) {
        cmd.exe /c "`"$VS_ENV_SCRIPT`" $HOST_TARGET & set" | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)$') {
                [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], [System.EnvironmentVariableTarget]::Process)
            }
        }
    }
}

function setup_dependencies {
    Write-Host -ForegroundColor Green "Setup dependencies..." 

    # Qt
    #   Docs on cloning and getting submodules
    #     https://doc.qt.io/qt-6.8/getting-sources-from-git.html
    #  Docs on configuring
    #     https://doc.qt.io/qt-6.8/configure-options.html
    #  Docs on building
    #     https://doc.qt.io/qt-6.8/build-sources.html
    $qt_dir = Join-Path $REPO_DIR "qt_$QT_TAG"
    $qt_source_dir = Join-Path $qt_dir "source"
    $qt_build_dir = Join-Path $qt_dir ("build_" + $TARGET_ARCH)
    $qt_install_dir = Join-Path $qt_dir ("install_" + $TARGET_ARCH)

    # Clone Qt
    Write-Host -ForegroundColor Green "    Clone Qt..."
    $qt_source_exists = Test-Path $qt_source_dir
    if ($qt_source_exists) {
        Write-Host -ForegroundColor Green "        Skipping clone, directory already exists: $qt_source_dir" 
    }
    else {
        echo_command "git clone --branch $QT_TAG git://code.qt.io/qt/qt5.git $qt_source_dir -c advice.detachedHead=false"
    }
    
    # Retrieve qtbase submodules
    Write-Host -ForegroundColor Green "    Retrieve qtbase submodule..."
    $qtbase_src_dir = Join-Path $qt_source_dir qtbase src
    $qtbase_src_dir_exists = Test-Path $qtbase_src_dir
    if ($qtbase_src_dir_exists) {
        Write-Host -ForegroundColor Green "        Skipping retrieve qtbase submodules, qtbase src exists: $qtbase_src_dir" 
    }
    else {
        $init_repository = Join-Path $qt_source_dir "init-repository"
        echo_command "Push-Location $qt_source_dir"
        echo_command "cmd /c $init_repository --module-subset=qtbase"
        echo_command "Pop-Location"
    }
     
    # Configure Qt    
    Write-Host -ForegroundColor Green "    Configure Qt..."
    $qt_build_dir_exists = Test-Path $qt_build_dir
    if ($qt_build_dir_exists) {
        Write-Host -ForegroundColor Green "        Skipping configure Qt, Qt build directory exists: $qt_build_dir" 
    }
    else {
        $configure = Join-Path $qt_source_dir "configure"
        echo_command "Push-Location $qt_source_dir"
        echo_command "cmd /c $configure -prefix $qt_install_dir -debug-and-release -static -static-runtime -- -Wno-dev -S $qt_source_dir -B $qt_build_dir"
        echo_command "Pop-Location"
    }

    # Build Qt
    Write-Host -ForegroundColor Green "    Build Qt..."
    $qt6widgetsd_library = Join-Path $qt_build_dir qtbase lib Qt6Widgetsd.lib
    $qt6widgetsd_library_exists = Test-Path $qt6widgetsd_library
    if ($qt6widgetsd_library_exists) {
         Write-Host -ForegroundColor Green "        Skipping build Qt, Qt6widgetsd library exists: $qt6widgetsd_library" 
    }
    else {
        echo_command "cmake --build $qt_build_dir --parallel"
    }
    
    # Install Qt
    Write-Host -ForegroundColor Green "    Install Qt..."
    $qt_install_dir_exists = Test-Path $qt_install_dir
    if ($qt_install_dir_exists) {
         Write-Host -ForegroundColor Green "        Skipping install Qt, install directory exists: $qt_install_dir" 
    }
    else {
        echo_command "ninja -C $qt_build_dir install"  # Qt will be installed into 'C:/Users/david/git/qt_v6.8.3/install'

    }
}

check_prerequisites
setup_build_environment
setup_dependencies