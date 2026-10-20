#!/usr/bin/env bash
# Create Source0 tarball from git, run rpmbuild -ba (openSUSE), copy installable RPM to dist/.
#
# Usage:
#   packaging/opensuse/build-rpm.sh           # Version from cpp/CMakeLists.txt
#   packaging/opensuse/build-rpm.sh v0.2.0  # explicit (CI passes git tag)
#   VERSION=0.2.0 packaging/opensuse/build-rpm.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

cmake_version() {
  sed -n 's/^project(p2td VERSION \([0-9.]*\).*/\1/p' "${ROOT}/cpp/CMakeLists.txt"
}

# RPM Version: no hyphens in the Version field.
rpm_version() {
  local v="$1"
  v="${v#v}"
  if [[ "$v" == *"-"* ]]; then
    echo "${v//-/.}"
  else
    echo "$v"
  fi
}

RAW_VERSION="${1:-${VERSION:-$(cmake_version)}}"
if [[ -z "${RAW_VERSION}" ]]; then
  echo "Could not determine version (pass arg, set VERSION, or set project(VERSION) in cpp/CMakeLists.txt)" >&2
  exit 1
fi
RPM_VERSION="$(rpm_version "${RAW_VERSION}")"
echo "p2td RPM Version: ${RPM_VERSION} (from: ${RAW_VERSION})"

RPM_TOP="${ROOT}/dist/rpm"
mkdir -p "${RPM_TOP}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

NAME="p2td-${RPM_VERSION}"
TARBALL="${NAME}.tar.gz"
git -C "$ROOT" archive --format=tar.gz --prefix="${NAME}/" -o "${RPM_TOP}/SOURCES/${TARBALL}" HEAD

rpmbuild -ba \
  --define "_topdir ${RPM_TOP}" \
  --define "p2td_version ${RPM_VERSION}" \
  "${ROOT}/packaging/opensuse/p2td.spec"

ARCH_DIR="$(uname -m)"
RPM="$(find "${RPM_TOP}/RPMS" -type f -name 'p2td-*.rpm' \
  ! -name '*debuginfo*' ! -name '*debugsource*' ! -name '*-test-*' \
  | sort | tail -1)"
if [[ -z "${RPM}" || ! -f "${RPM}" ]]; then
  echo "No main p2td binary RPM under dist/rpm/RPMS" >&2
  exit 1
fi

ARCH="$(rpm -qp --qf '%{ARCH}' "${RPM}" 2>/dev/null || echo "${ARCH_DIR}")"
TAG_LABEL="${RAW_VERSION}"
if [[ "${TAG_LABEL}" != v* ]]; then
  TAG_LABEL="v${TAG_LABEL#v}"
fi
mkdir -p "${ROOT}/dist"
DEST="${ROOT}/dist/p2td-${TAG_LABEL}-opensuse-tumbleweed.${ARCH}.rpm"
cp -f "${RPM}" "${DEST}"

echo "RPM build tree: ${RPM_TOP}/RPMS/${ARCH_DIR}/"
echo "Installable RPM: ${DEST}"
