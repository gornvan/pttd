#!/usr/bin/env bash
# Build openSUSE RPM for a release tag or version (CI or local on openSUSE).
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TAG_OR_VERSION="${1:?usage: build-opensuse-rpm.sh v0.1.0}"

bash "${REPO_ROOT}/packaging/opensuse/build-rpm.sh" "${TAG_OR_VERSION}"

RPM="$(find "${REPO_ROOT}/dist/rpm/RPMS" -name 'p2td-*.rpm' -type f | sort | tail -1)"
if [[ -z "${RPM}" || ! -f "${RPM}" ]]; then
  echo "No p2td RPM under dist/rpm/RPMS" >&2
  exit 1
fi

ARCH="$(rpm -qp --qf '%{ARCH}' "${RPM}" 2>/dev/null || basename "$(dirname "${RPM}")")"
TAG_NAME="${TAG_OR_VERSION#v}"
if [[ "${TAG_OR_VERSION}" == v* ]]; then
  OUT_NAME="p2td-${TAG_OR_VERSION}-opensuse-tumbleweed.${ARCH}.rpm"
else
  OUT_NAME="p2td-v${TAG_NAME}-opensuse-tumbleweed.${ARCH}.rpm"
fi

mkdir -p "${REPO_ROOT}/dist"
DEST="${REPO_ROOT}/dist/${OUT_NAME}"
cp -f "${RPM}" "${DEST}"
echo "Release RPM: ${DEST}"
