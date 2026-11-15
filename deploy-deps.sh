#!/bin/bash
# Copies every MSYS2 UCRT64 DLL that LogBook.exe and its Qt plugins need into the given folder
# (default: build/). Usage: deploy-deps.sh [folder]
export PATH=/ucrt64/bin:/usr/bin:$PATH
cd "$(dirname "$0")" || exit 1
cd "${1:-build}" || exit 1
for f in LogBook.exe *.dll */*.dll; do ldd "$f" 2>/dev/null; done \
  | awk '$3 ~ /ucrt64/ {print $3}' | sort -u \
  | while read -r dll; do cp -u "$dll" . ; done
