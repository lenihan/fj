# FJ

fj is a keyboard-only, home-row-navigated digital index-card app. It
builds natively on Windows and Linux, and to WebAssembly for running in a
browser -- no Qt or other third-party dependency on any of the three (see
`PLAN.md`'s ARCHITECTURE section). Pick the section below for your
platform.

## Windows

### Prerequisites

* PowerShell 7+
  * `winget install --id Microsoft.PowerShell -e --source winget`
* git
  * `winget install --id Git.Git -e --source winget`
* CMake 3.25+
  * `winget install --id Kitware.CMake -e --source winget`
* Visual Studio 2022
  * `winget install --id Microsoft.VisualStudio.2022.Community -e --source winget`
  * Choose workload "Desktop development with C++"

### Build

```pwsh
git clone https://github.com/lenihan/fj
cd fj
cmake --workflow --preset windows-x64-debug
```

(`windows-x64-release`, `windows-arm64-debug`, `windows-arm64-release`
also exist -- see `CMakePresets.json`.)

### Run

```pwsh
./build/windows-x64/Debug/fj.exe
```

## Linux

### Prerequisites

CMake 3.25+, Ninja, a C++23 compiler, and the X11/XShm/XRandr dev headers
`xlibWindow.cpp` builds against. On Debian/Ubuntu:

```sh
sudo apt install cmake ninja-build g++ libx11-dev libxext-dev libxrandr-dev
```

(other distros: the equivalent packages for the same libraries)

### Build

```sh
git clone https://github.com/lenihan/fj
cd fj
cmake --workflow --preset linux-debug
```

(`linux-release` also exists.)

### Run

```sh
./build/linux/Debug/fj
```

## Web

Builds `fj` to WebAssembly via Emscripten -- runs in any browser, no
server-side code. See `PLAN.md`'s "Web (Emscripten) shell" section for
the design; `web-debug`, in particular, is *much* slower to interact
with than a native Debug build (unoptimized wasm, not this app) -- use
`web-release` to actually try it out, and reach for `web-debug` only when
you need to step through the wasm itself in the browser's debugger.

### Prerequisites

[emsdk](https://github.com/emscripten-core/emsdk) (the Emscripten SDK)
and Ninja, both obtained via emsdk itself:

```sh
git clone https://github.com/emscripten-core/emsdk
cd emsdk
./emsdk install latest
./emsdk install ninja-1.13.2-64bit
./emsdk activate latest
./emsdk activate ninja-1.13.2-64bit
```

Each new shell needs `emcc`/`ninja` on `PATH` and `EMSDK` set before
configuring fj. The usual way is sourcing emsdk's own env script:

```sh
source ./emsdk_env.sh          # Linux/macOS
```

```pwsh
. .\emsdk_env.ps1               # Windows -- see caveat below
```

On Windows, `emsdk_env.ps1` (and `emsdk activate --permanent`) did not
reliably add `upstream\emscripten` -- where `emcc` actually lives -- to
`PATH` when this was set up (see `PLAN.md`). If `emcc --version` still
fails to find anything after sourcing it, set `PATH` directly instead:

```pwsh
$env:EMSDK = "<path to emsdk>"
$env:PATH = "$env:EMSDK;$env:EMSDK\upstream\emscripten;$env:EMSDK\ninja\1.13.2_64bit;$env:PATH"
```

### Build

```sh
git clone https://github.com/lenihan/fj
cd fj
cmake --workflow --preset web-release
```

(`web-debug` also exists -- see the speed caveat above.)

### Run

`.wasm` needs to be served over HTTP, not opened directly as a `file://`
URL:

```sh
cd build/web/Release
python -m http.server 8000
```

Then open `http://localhost:8000/fj.html` in a browser.
