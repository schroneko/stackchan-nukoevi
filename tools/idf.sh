#!/bin/zsh
set -eu

script_dir="${0:A:h}"
repo_root="${script_dir:h}"
firmware_dir="${repo_root}/firmware"

candidate_paths=()
if [ -n "${IDF_PATH:-}" ]
then
  candidate_paths+=("${IDF_PATH}")
fi
if [ -n "${ESP_IDF_PATH:-}" ]
then
  candidate_paths+=("${ESP_IDF_PATH}")
fi
candidate_paths+=("${GHQ_ROOT:-${HOME}/ghq}/github.com/espressif/esp-idf")
candidate_paths+=("${HOME}/esp/esp-idf")

idf_path=""
for candidate in "${candidate_paths[@]}"
do
  if [ -f "${candidate}/export.sh" ]
  then
    idf_path="${candidate}"
    break
  fi
done

if [ -z "${idf_path}" ]
then
  print -u2 "ESP-IDF export.sh not found. Set IDF_PATH or install ESP-IDF under ${GHQ_ROOT:-${HOME}/ghq}/github.com/espressif/esp-idf."
  exit 1
fi

. "${idf_path}/export.sh" >/dev/null 2>&1
cd "${firmware_dir}"
exec "${IDF_PATH}/tools/idf.py" "$@"
