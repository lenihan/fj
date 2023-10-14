To generate solution:
cd ~/repos/fj/minibrowser
cmake -S. -Bbuild -DCMAKE_PREFIX_PATH=C:\Qt\6.5.2\msvc2019_64

Build in Visual Studio

To run:
$env:Path += ";C:\Qt\6.5.2\msvc2019_64\bin"
.\build\Debug\