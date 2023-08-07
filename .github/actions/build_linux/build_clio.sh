#!/usr/bin/env bash
docker run --rm -it -v $PWD:/home/conan/clio --workdir="/home/conan/clio" --entrypoint ./.github/actions/build_linux/build_clio.sh  conanio/gcc11:1.60.2
conan profile new default --detect
conan profile update settings.compiler.cppstd=20 default
conan profile update settings.compiler.libcxx=libstdc++11 default
conan export external/cassandra
conan remote add --insert 0 conan-non-prod http://18.143.149.228:8081/artifactory/api/conan/conan-non-prod
conan install . -if build -of build  --build missing  --settings build_type=Release  -o tests=True
cmake -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build --parallel $(($(nproc) - 2))
