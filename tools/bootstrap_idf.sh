#!/usr/bin/env bash
set -euo pipefail
IDC_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
IDC_SDK_ROOT=/tmp/id-card-esp-idf
IDC_TOOLS_ROOT=/tmp/id-card-idf-tools
IDC_PY_ROOT=/tmp/id-card-idf-python
IDC_UTILS_ROOT=/tmp/id-card-build-utils
IDC_PYTHON=${PYTHON_BIN:-python3}
if command -v python3.12 >/dev/null 2>&1; then IDC_PYTHON=${PYTHON_BIN:-python3.12}; fi
if [ ! -d "$IDC_SDK_ROOT/.git" ]; then git clone --filter=blob:none --no-checkout https://github.com/espressif/esp-idf.git "$IDC_SDK_ROOT"; fi
if [ -n "$(git -C "$IDC_SDK_ROOT" status --porcelain)" ]; then
  echo 'Existing SDK has changes. Preserve them; choose a clean SDK before continuing.' >&2
  exit 1
fi
git -C "$IDC_SDK_ROOT" checkout "$IDC_COMMIT"
git -C "$IDC_SDK_ROOT" submodule update --init --recursive --depth 1
mkdir -p "$IDC_PY_ROOT"
export IDF_TOOLS_PATH="$IDC_TOOLS_ROOT"
export IDF_PYTHON_ENV_PATH="$IDC_PY_ROOT"
"$IDC_PYTHON" "$IDC_SDK_ROOT/tools/idf_tools.py" install --targets=esp32s31
"$IDC_PYTHON" "$IDC_SDK_ROOT/tools/idf_tools.py" install-python-env --features=core
if [ ! -x "$IDC_UTILS_ROOT/bin/python" ]; then "$IDC_PYTHON" -m venv "$IDC_UTILS_ROOT"; fi
"$IDC_UTILS_ROOT/bin/python" -m pip install cmake==4.4.5 ninja==1.13.2
printf '%s\n' 'SDK ready. No device accessed or flashed.' \
 'Run: export IDF_TOOLS_PATH=/tmp/id-card-idf-tools IDF_PYTHON_ENV_PATH=/tmp/id-card-idf-python' \
 '. /tmp/id-card-esp-idf/export.sh' \
 'python3 tools/build_portable.py --project e6 --profile default'
