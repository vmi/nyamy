#!/bin/bash

set -eu

help() {
  echo "Usage: $0 PSFile [arg ...]"
  exit 1
}

if [ $# -eq 0 ]; then
  help
fi

# --- Get the PS script path (first argument) ---
psfile="$(cygpath -aw "$1")"
shift

# --- Collect the remaining arguments, preserving spaces/special chars ---
psargs=()
for arg in "$@"; do
  case "$arg" in
    */*)
      # path name
      arg="$(cygpath -aw "$arg")"
      ;;
  esac
  psargs+=( "$arg" )
done

# --- Locate PowerShell host: prefer pwsh, then powershell ---
find_pshost() {
  if [ -n "${PSHOST:-}" ]; then
    pshost="$(cygpath -u "$PSHOST")"
    return
  fi
  pshost="$(which pwsh 2>/dev/null)" || true
  if [ -n "$pshost" ]; then
    return
  fi
  local pwsh_exe="$(cygpath -u "$PROGRAMFILES"'\PowerShell\7\pwsh.exe')"
  if [ -x "$pwsh_exe" ]; then
    pshost="$pwsh_exe"
    return
  fi
  pshost="$(which powershell 2>/dev/null)" || true
  if [ -n "$pshost" ]; then
    return
  fi
  local powershell_exe="$(cygpath -u "$SYSTEMROOT"'\System32\WindowsPowerShell\v1.0\powershell.exe')"
  if [ -x "$powershell_exe" ]; then
    pshost="$powershell_exe"
    return
  fi
  echo "[ERROR] PowerShell executable was not found."
  exit 1
}

find_pshost

# --- Execute: -NoProfile with temporary ExecutionPolicy Bypass ---
exec "$pshost" -NoProfile -ExecutionPolicy Bypass -File "$psfile" "${psargs[@]}"
