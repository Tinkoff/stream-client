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
    if [[ $# -lt 2 ]]; then
        echo "Build folder and/or build type are absent"
        return 1
    fi

    declare -r folder="$1"
    declare -r build_type="$2"

    build "$folder" "testing" "-DCMAKE_BUILD_TYPE=$build_type" \
                              "-DSTREAMCLIENT_BUILD_TESTING=ON" \
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
    build_testing "build-tsan" "Tsan"
    build_testing "build-asan" "Asan"
    build_testing "build-ubsan" "Ubsan"
    build_testing "build" "Debug"

elif [[ "$1" == "test-type" ]] ; then
    if [[ $# -lt 2 ]]; then
        echo "Build type is not set"
        exit 1
    fi
    build_testing "build-$2" "$2"

elif [[ "$1" == "coverage" ]] ; then
    declare -r build_folder="${2:-build}"
    build_testing "$build_folder" "Debug"
    collect_coverage "$build_folder"

elif [[ "$1" == "collect-coverage" ]] ; then
    declare -r build_folder="${2:-build}"
    collect_coverage "$build_folder"

elif [[ "$1" == "docs" ]] ; then
    build "build" "docs" "-DSTREAMCLIENT_BUILD_TESTING=OFF" \
                         "-DSTREAMCLIENT_BUILD_DOCS=ON"

elif [[ "$1" == "lint" ]] ; then
    declare -r build_folder="${2:-build}"
    if [[ $# -eq 1 ]]; then
        shift 1
    else
        shift 2
    fi
    ./lint.py "--color=always"\
              "--style=file"\
              "--exclude-tidy=*.ipp"\
              "--build-path=$build_folder"\
              "--recursive"\
              "$@"\
              include/

elif [[ "$1" == "build" ]] ; then
    shift 1
    build "build" "all" "$@"

else
    printf "Usage: $0 (command)
Where command can be one of:
    test                            - build and run unit tests with all sanitizers
    test-type <build type>          - build and run tests under single build type
    coverage [build folder]         - build 'Debug', run test and report coverage
    collect-coverage [build folder] - collect coverage from previous build
    lint [build folder] [args]      - lint previous build, args passed to the lint.py as is
    docs                            - build HTML docs
    build [args]                    - build and pass args to cmake as is\n"
fi
