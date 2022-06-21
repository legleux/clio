#!/usr/bin/env bash
### y'know what? just install them all? make it a switch?
IFS=- read -r CC VER <<< $1

echo "Installing ${CC}-${VER}"
# if [ $CC = gcc ];then
#     if [ $VERSION = 11 ]; then
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get update && apt-get install -y gcc-11 g++-11
update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 110 \
    --slave /usr/bin/g++ g++ /usr/bin/g++-11 \
    --slave /usr/bin/gcov gcov /usr/bin/gcov-11 \
    --slave /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-11 \
    --slave /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-11
apt-get -y install clang clang-12

#     elif [$VERSION = 12]

#         # build gcc 12
#         git clone https://gcc.gnu.org/git/gcc.git gcc-source
#         cd gcc-source/
#         git branch -a
#         git checkout remotes/origin/releases/gcc-12

#         mkdir ../gcc-12-build
#         cd ../gcc-12-build/
#         ./../gcc-source/configure --prefix=$HOME/install/gcc-12 --enable-languages=c,c++

#         apt-cache search MPFR
#         apt-get install libmpfrc++-dev
#         apt-cache search MPC | grep dev
#         apt-get install libmpc-dev
#         apt-cache search GMP | grep dev
#         apt-get install libgmp-dev
#         apt-get install gcc-multilib
#         ./../gcc-source/configure --prefix=$HOME/install/gcc-12 --enable-languages=c,c++

#         make -j16

#         apt-get install flex
#         ./../gcc-source/configure --prefix=$HOME/install/gcc-12 --enable-languages=c,c++
#         make -j16
#         make install
#     fi
#     elif [ $CC = clang ];then
#         if [ $VER = 10 ];then
#             apt-get install clang
#         elif [ $VER = 12 ];then
#             apt-get install clang-12
#         fi
#     fi
# fi
