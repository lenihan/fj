# To generate solution: 
#   NOTE: "-A x64" needed to make installer use "Program Files" instead of "Program Files (x86)
cd ~/repos/fj/contentmanipulation
cmake -S. -Bbuild -A x64

# To build debug
cmake --build .\build\ 
# To build release
cmake --build .\build\ --config Release  

# Install to C:/Program Files/contentmanipulation/examples/webenginewidgets/contentmanipulation
# (need admin terminal)
# Does not copy qt dll's
cmake --install .\build\

# Set path to find Qt .dll's
<!-- $env:Path += ";C:\Qt\6.5.2\msvc2019_64\bin" -->

# Build solution in Visual Studio, set active project as "contentmanipulation"
.\build\contentmanipulation.sln
