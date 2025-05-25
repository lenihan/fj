# Setup prerequisites, build environment, dependencies

#Requires -Version 7

# Inputs
$QT_TAG = "v6.8.3" # LTS - Long-Term Support
$ARCH = "arm64"
if ($IsWindows) {
    $VS_ENV_SCRIPT = Get-ChildItem -Recurse "$env:ProgramFiles\Microsoft Visual Studio\2022\*\Common7\Tools\Launch-VsDevShell.ps1"
}


# Globals
$ROOT_DIR = Resolve-Path $PSScriptRoot/..
$REPO_DIR = Resolve-Path $ROOT_DIR/..

# $VCPKG_DIR = Join-Path $REPO_DIR "vcpkg_$VCPKG_TAG"

function echo_command($cmd) {
    Write-Host -ForegroundColor Cyan $cmd 
    Invoke-Expression $cmd
}

function check_prerequisites {
    Write-Host -ForegroundColor Green "Checking prerequisites..."

    if ($IsWindows) {
        $required = "git", "cmake", "code", $VS_ENV_SCRIPT
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
        & $VS_ENV_SCRIPT -Arch $ARCH -SkipAutomaticLocation
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
    $qt_build_dir = Join-Path $qt_dir "build"
    $qt_install_dir = Join-Path $qt_dir "install"

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