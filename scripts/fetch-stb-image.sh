#!/usr/bin/env bash
set -euo pipefail

# stb_image is fetched at a fixed upstream commit. The NRO does not use the
# network at runtime; this script only supplies the compile-time decoder.
STB_COMMIT="9600522de53973336fa033a289858a4354cbb04c"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${ROOT}/third_party/stb"
HEADER="${DEST}/stb_image.h"
TMP="${HEADER}.tmp"
URL="https://raw.githubusercontent.com/nothings/stb/${STB_COMMIT}/stb_image.h"

mkdir -p "${DEST}"

if [[ -s "${HEADER}" ]]; then
    exit 0
fi

rm -f "${TMP}"

if command -v curl >/dev/null 2>&1; then
    curl --fail --location --silent --show-error --retry 3 "${URL}" --output "${TMP}"
elif command -v wget >/dev/null 2>&1; then
    wget --quiet "${URL}" --output-document="${TMP}"
elif command -v python3 >/dev/null 2>&1; then
    python3 - "${URL}" "${TMP}" <<'PY'
import sys
import urllib.request

url, output = sys.argv[1], sys.argv[2]
request = urllib.request.Request(url, headers={"User-Agent": "RandomNROIdeas-build"})
with urllib.request.urlopen(request, timeout=30) as response, open(output, "wb") as target:
    target.write(response.read())
PY
else
    echo "AlbumSigner dependency fetch requires curl, wget, or python3." >&2
    exit 1
fi

if ! grep -q "stbi_load" "${TMP}"; then
    echo "Downloaded stb_image header did not pass its sanity check." >&2
    rm -f "${TMP}"
    exit 1
fi

mv "${TMP}" "${HEADER}"
echo "Prepared stb_image.h from nothings/stb commit ${STB_COMMIT}."
