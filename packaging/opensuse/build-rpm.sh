#!/usr/bin/env bash
# Create Source0 tarball from git and run rpmbuild -ba (openSUSE).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
VERSION="$(sed -n 's/^project(p2td VERSION \([0-9.]*\).*/\1/p' "$ROOT/cpp/CMakeLists.txt")"
if [[ -z "$VERSION" ]]; then
  echo "Could not read version from cpp/CMakeLists.txt" >&2
  exit 1
fi

RPM_TOP="${ROOT}/dist/rpm"
mkdir -p "${RPM_TOP}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

NAME="p2td-${VERSION}"
TARBALL="${NAME}.tar.gz"
git -C "$ROOT" archive --format=tar.gz --prefix="${NAME}/" -o "${RPM_TOP}/SOURCES/${TARBALL}" HEAD

rpmbuild -ba --define "_topdir ${RPM_TOP}" "${ROOT}/packaging/opensuse/p2td.spec"

echo "RPM(s): ${RPM_TOP}/RPMS/$(uname -m)/"
