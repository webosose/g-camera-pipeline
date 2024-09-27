#!/usr/bin/env bash
[ $(which clang-format-3.9) ] && CLANG_FORMAT=clang-format-3.9 || CLANG_FORMAT=clang-format
echo "======================"
echo "Formatting code for..."
echo "======================"
git log -1 --name-only
echo "======================"
git log -1 --name-only --pretty=format:"" | grep "cpp\|hpp\|.h" | sed ':a;N;$!ba;s/\n/ /g' | xargs ${CLANG_FORMAT} -i
