# FJ

## Prerequisites

* Windows
  * PowerShell 7+
    * `winget install --id Microsoft.PowerShell -e --source winget`
  * git
    * `winget install --id Git.Git -e --source winget`
  * cmake
    * `winget install --id Kitware.CMake -e --source winget`
  * Visual Studio 2022
    * `winget install --id Microsoft.VisualStudio.2022.Community -e --source winget`
    * Choose workload "Desktop development with C++"

## Setup

Build dependencies. ~20 minutes

```pwsh
> cd <GIT REPOSITORY DIR>
> git clone https://github.com/lenihan/fj
> cd fj
> ./scripts/setup.ps1
```

## Build FJ

```pwsh
> cd <GIT REPOSITORY DIR>/fj
> cmake -S . -B build  # Create build files
> cmake --build build  # Build build files
```

## Run

```pwsh
> ./<GIT REPOSITORY DIR>/fj/build/Debug/fj
````
