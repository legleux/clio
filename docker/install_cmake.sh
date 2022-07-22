
CMAKE_VERSION=$1
# IFS=. read -r MAJ MIN <<< $CMAKE_FULL_VERSION
# MAJ=$(echo $CMAKE_FULL_VERSION | tr "." " " | cut -d ' ' -f 1)
# MIN=$(echo $CMAKE_FULL_VERSION | tr "." " " | cut -d ' ' -f 2)
# P=$(echo $CMAKE_FULL_VERSION | tr "." " " | cut -d ' ' -f 3)
# CMAKE_VERSION=$MAJ.$MIN
URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
curl -OJLs $URL
tar xzvf cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
ln -s /cmake-${CMAKE_VERSION}-Linux-x86_64/bin/cmake /usr/local/bin/cmake

# https://github.com/Kitware/CMake/releases/download/v3.16.3/cmake-3.16.3-Linux-x86_64.tar.gz
