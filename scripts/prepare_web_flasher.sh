#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 BUILD_DIRECTORY OUTPUT_DIRECTORY" >&2
  exit 64
fi

build_dir="$1"
output_dir="$2"
project_name="bb-link.ino"
merged_binary="${build_dir}/${project_name}.merged.bin"

if [[ ! -f "${merged_binary}" ]]; then
  echo "Merged firmware not found: ${merged_binary}" >&2
  exit 66
fi

read_define() {
  local name="$1"
  local value
  value="$(awk -v name="${name}" '$1 == "#define" && $2 == name { print $3 }' src/bb-link/Adapter.h)"
  if [[ ! "${value}" =~ ^[0-9]+$ ]]; then
    echo "Unable to read numeric ${name} from src/bb-link/Adapter.h" >&2
    exit 65
  fi
  printf '%s' "${value}"
}

version="$(read_define FIRMWARE_VERSION_MAJOR).$(read_define FIRMWARE_VERSION_MINOR).$(read_define FIRMWARE_VERSION_PATCH)"
firmware_file="bb-link-atomlite-${version}-factory.bin"

mkdir -p "${output_dir}/firmware"
cp web/index.html "${output_dir}/index.html"
cp web/styles.css "${output_dir}/styles.css"
cp web/.nojekyll "${output_dir}/.nojekyll"
cp assets/atomlite.webp "${output_dir}/atomlite.webp"
cp "${merged_binary}" "${output_dir}/firmware/${firmware_file}"

sed \
  -e "s/@FIRMWARE_VERSION@/${version}/g" \
  -e "s/@FIRMWARE_FILE@/${firmware_file}/g" \
  web/manifest.json.in > "${output_dir}/manifest.json"

sed \
  -e "s/@FIRMWARE_VERSION@/${version}/g" \
  "${output_dir}/index.html" > "${output_dir}/index.html.tmp"
mv "${output_dir}/index.html.tmp" "${output_dir}/index.html"

sha256sum "${output_dir}/firmware/${firmware_file}" \
  | cut -d ' ' -f 1 \
  > "${output_dir}/firmware/${firmware_file}.sha256.txt"

echo "Prepared B.B. Link ATOM Lite Web Flasher v${version} in ${output_dir}"
