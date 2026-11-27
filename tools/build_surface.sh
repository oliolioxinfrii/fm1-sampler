#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
base="${FM1_BASE_DIR:-$root/.fm1-base}"
pin=f4bd9c5798f7e398f7a93e0d891f7d878121f1de
if [[ ! -d "$base/.git" && ! -f "$base/.git" ]]; then
  git clone --recursive https://github.com/charlesvestal/fm1-base "$base"
fi
if ! git -C "$base" cat-file -e "$pin^{commit}"; then git -C "$base" fetch origin "$pin"; fi
if [[ "$(git -C "$base" rev-parse HEAD)" != "$pin" ]]; then
  if [[ -n "$(git -C "$base" status --porcelain)" ]]; then echo "fm1-base checkout is dirty; use a clean pinned checkout" >&2; exit 1; fi
  git -C "$base" checkout --detach "$pin"
fi
git -C "$base" submodule update --init --recursive
python3 "$base/tools/build.py" "$root/surface-firmware" "$@"
