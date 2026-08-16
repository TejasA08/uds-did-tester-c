#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
make -j
if [[ ! -f config/setup.csv ]]; then
  ./build/uds_tester --create-config
  echo "Edit config/setup.csv and config/test_cases.csv, then rerun."
  exit 0
fi
./build/uds_tester "$@"
