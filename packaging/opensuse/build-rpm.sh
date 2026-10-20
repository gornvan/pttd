#!/usr/bin/env bash
# Create Source0 tarball from git and run rpmbuild -ba (openSUSE).
#
# Usage:
#   packaging/opensuse/build-rpm.sh              # RPM Version 0~debug.<timestamp>
#   packaging/opensuse/build-rpm.sh 0.2.0        # explicit version
#   VERSION=1.0.0 packaging/opensuse/build-rpm.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

# RPM Version: no hyphens (use Release for that). Map debug-* hand-test ids to 0~debug.<stamp>.
rpm_version() {
  local v="$1"
  v="${v#v}"
  if [[ "$v" =~ ^debug-([0-9]+)$ ]]; then
    echo "0~debug.${BASH_REMATCH[1]}"
  elif [[ "$v" =~ ^debug([0-9]+)$ ]]; then
    echo "0~debug.${BASH_REMATCH[1]}"
  elif [[ "$v" == *"-"* ]]; then
    echo "${v//-/.}"
  else
    echo "$v"
  fi
}

RAW_VERSION="${1:-${VERSION:-debug-$(date +%Y%m%d%H%M%S)}}"
if [[ -z "${RAW_VERSION}" ]]; then
  echo "VERSION is empty" >&2
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

echo "RPM(s): ${RPM_TOP}/RPMS/$(uname -m)/"
