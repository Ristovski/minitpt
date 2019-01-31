#!/bin/sh

cd "${MESON_BUILD_ROOT}"
cppcheck -j 4 --enable=all --project=${MESON_BUILD_ROOT}/compile_commands.json 2> static_report
