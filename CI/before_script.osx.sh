#!/bin/sh -e

export CXX=clang++
export CC=clang

if [[ "${BUILD_TESTS_ONLY}" ]]; then
    export GOOGLETEST_DIR="${PWD}/googletest/build/install"
    env GENERATOR='Unix Makefiles' CONFIGURATION=Release CI/build_googletest.sh
fi

DEPENDENCIES_ROOT="/private/tmp/openmw-deps/openmw-deps"
QT_PATH=$(brew --prefix qt@5)
CCACHE_EXECUTABLE=$(brew --prefix ccache)/bin/ccache
mkdir build
cd build

if [[ "${BUILD_TESTS_ONLY}" ]]; then
    cmake \
        -D CMAKE_PREFIX_PATH="$DEPENDENCIES_ROOT;$QT_PATH" \
        -D CMAKE_C_COMPILER_LAUNCHER="$CCACHE_EXECUTABLE" \
        -D CMAKE_CXX_COMPILER_LAUNCHER="$CCACHE_EXECUTABLE" \
        -D CMAKE_CXX_FLAGS="-stdlib=libc++" \
        -D CMAKE_C_FLAGS_RELEASE="-g -O0" \
        -D CMAKE_CXX_FLAGS_RELEASE="-g -O0" \
        -D CMAKE_OSX_DEPLOYMENT_TARGET="10.12" \
        -D CMAKE_BUILD_TYPE=Release \
        -D OPENMW_OSX_DEPLOYMENT=ON \
        -D BUILD_OPENMW=OFF \
        -D BUILD_OPENCS=OFF \
        -D BUILD_ESMTOOL=OFF \
        -D BUILD_BSATOOL=OFF \
        -D BUILD_ESSIMPORTER=OFF \
        -D BUILD_NIFTEST=OFF \
        -D BUILD_UNITTESTS=ON \
        -D GTEST_ROOT="${GOOGLETEST_DIR}" \
        -D GMOCK_ROOT="${GOOGLETEST_DIR}" \
        -D BULLET_USE_DOUBLES=ON \
        -G "Unix Makefiles" \
        ..
elif [[ "${BUILD_ENGINE_ONLY}" ]]; then
    cmake \
        -D CMAKE_PREFIX_PATH="$DEPENDENCIES_ROOT;$QT_PATH" \
        -D CMAKE_C_COMPILER_LAUNCHER="$CCACHE_EXECUTABLE" \
        -D CMAKE_CXX_COMPILER_LAUNCHER="$CCACHE_EXECUTABLE" \
        -D CMAKE_CXX_FLAGS="-stdlib=libc++" \
        -D CMAKE_C_FLAGS_RELEASE="-g -O0" \
        -D CMAKE_CXX_FLAGS_RELEASE="-g -O0" \
        -D CMAKE_OSX_DEPLOYMENT_TARGET="10.12" \
        -D CMAKE_BUILD_TYPE=Release \
        -D OPENMW_OSX_DEPLOYMENT=ON \
        -D BUILD_OPENMW=TRUE \
        -D BUILD_OPENCS=OFF \
        -D BUILD_ESMTOOL=OFF \
        -D BUILD_BSATOOL=OFF \
        -D BUILD_ESSIMPORTER=OFF \
        -D BUILD_NIFTEST=OFF \
        -D BULLET_USE_DOUBLES=ON \
        -G "Unix Makefiles" \
        ..
else
    cmake \
        -D CMAKE_PREFIX_PATH="$DEPENDENCIES_ROOT;$QT_PATH" \
        -D CMAKE_C_COMPILER_LAUNCHER="$CCACHE_EXECUTABLE" \
        -D CMAKE_CXX_COMPILER_LAUNCHER="$CCACHE_EXECUTABLE" \
        -D CMAKE_CXX_FLAGS="-stdlib=libc++" \
        -D CMAKE_C_FLAGS_RELEASE="-g -O0" \
        -D CMAKE_CXX_FLAGS_RELEASE="-g -O0" \
        -D CMAKE_OSX_DEPLOYMENT_TARGET="10.12" \
        -D CMAKE_BUILD_TYPE=Release \
        -D OPENMW_OSX_DEPLOYMENT=TRUE \
        -D BUILD_OPENMW=ON \
        -D BUILD_OPENCS=ON \
        -D BUILD_ESMTOOL=ON \
        -D BUILD_BSATOOL=ON \
        -D BUILD_ESSIMPORTER=ON \
        -D BUILD_NIFTEST=ON \
        -D BUILD_LAUNCHER=ON \
        -D BUILD_MWINIIMPORTER=ON \
        -D BUILD_WIZARD=ON \
        -D BULLET_USE_DOUBLES=TRUE \
        -G "Unix Makefiles" \
        ..
fi
