#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SUBMODULE_DIR="${ROOT_DIR}/third_party/qlementine"
PATCH_FILE="${ROOT_DIR}/patches/qlementine/0001-guard-widget-animation-manager-on-destroy.patch"

if [[ ! -d "${SUBMODULE_DIR}/.git" && ! -f "${SUBMODULE_DIR}/.git" ]]; then
  echo "qlementine submodule is missing: ${SUBMODULE_DIR}" >&2
  exit 1
fi

if git -C "${SUBMODULE_DIR}" apply --check "${PATCH_FILE}" >/dev/null 2>&1; then
  git -C "${SUBMODULE_DIR}" apply "${PATCH_FILE}"
  echo "Applied qlementine patch"
  exit 0
fi

if git -C "${SUBMODULE_DIR}" apply --reverse --check "${PATCH_FILE}" >/dev/null 2>&1; then
  echo "Patch already applied"
  exit 0
fi

echo "Patch cannot be applied cleanly: ${PATCH_FILE}" >&2
exit 1
