#!/usr/bin/env bash

[[ "$TRACE" ]] && set -o xtrace
set -o pipefail
set -o errexit

NUM_PROC=$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)

build() {
    if [[ $# -lt 2 ]]; then
        echo "Build folder or target are absent"
        return 1
    fi

    declare -r folder="$1"
    declare -r target="$2"
    shift 2

    cmake -H. -B"$folder" "$@"
    cmake --build "$folder" --target "$target" "--" "-j$NUM_PROC"
}

build_testing() {
    if [[ $# -lt 1 ]]; then
        echo "Build folder is absent"
        return 1
    fi

    declare -r folder="$1"

    build "$folder" "testing" "-DSTREAMCLIENT_BUILD_TESTING=ON" \
                              "-DSTREAMCLIENT_BUILD_DOCS=OFF" \
                              "-DSTREAMCLIENT_BUILD_EXAMPLES=OFF" \
                              "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"\
                              "-DCMAKE_CXX_OUTPUT_EXTENSION_REPLACE=ON"
}

collect_coverage() {
    if [[ $# -lt 1 ]]; then
        echo "Build folder is absent"
        return 1
    fi

    lcov --no-external --directory $(pwd) --capture --output-file coverage.info
    lcov --remove coverage.info -o coverage.info "*/tests/*" "*/$1/*"
}

if [[ "$1" == "test" ]] ; then
    build_testing "build"

elif [[ "$1" == "coverage" ]] ; then
    build_testing "build"
    collect_coverage "build"

elif [[ "$1" == "collect-coverage" ]] ; then
    collect_coverage "build"

elif [[ "$1" == "docs" ]] ; then
    build "build" "docs" "-DSTREAMCLIENT_BUILD_TESTING=OFF" \
                         "-DSTREAMCLIENT_BUILD_DOCS=ON"

elif [[ "$1" == "lint" ]] ; then
    shift 1
    ./lint.py --color=always --style=file --build-path=./build --recursive "$@" include/

elif [[ "$1" == "build" ]] ; then
    shift 1
    build "build" "all" "$@"

else
    echo "Usage: $0 [test|coverage|collect-coverage|docs|lint|build]"
fi
