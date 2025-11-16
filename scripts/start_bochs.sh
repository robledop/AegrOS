#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_TEMPLATE="$ROOT_DIR/scripts/bochsrc"
BOCHS_BIN="${BOCHS_BIN:-bochs}"
DISPLAY_LIBRARY="${BOCHS_DISPLAY_LIBRARY:-}"
DISPLAY_OPTIONS="${BOCHS_DISPLAY_OPTIONS:-}"
ROMIMAGE_PATH="${BOCHS_ROMIMAGE:-}"
VGAROM_PATH="${BOCHS_VGAROMIMAGE:-}"
BXSHARE_PATH="${BOCHS_BXSHARE:-${BXSHARE:-}}"
CPU_COUNT="${BOCHS_CPUS:-1}"
RUN_IMMEDIATE="${BOCHS_RUN_IMMEDIATE:-1}"
if ! [[ "$CPU_COUNT" =~ ^[0-9]+$ ]] || [[ "$CPU_COUNT" -lt 1 ]]; then
  CPU_COUNT=1
fi

detect_display_library() {
  declare -A found=()
  local -a search_dirs=()

  if [[ -n "${LTDL_LIBRARY_PATH:-}" ]]; then
    IFS=':' read -ra ltdl_dirs <<<"${LTDL_LIBRARY_PATH}"
    for dir in "${ltdl_dirs[@]}"; do
      [[ -n "$dir" ]] && search_dirs+=("$dir")
    done
  fi

  search_dirs+=(
    "/usr/lib/x86_64-linux-gnu/bochs/plugins"
    "/usr/lib64/bochs/plugins"
    "/usr/lib/bochs/plugins"
    "/usr/local/lib/bochs/plugins"
  )

  if [[ -n "${BOCHS_PATH:-}" ]]; then
    local prefix
    prefix="$(cd "$(dirname "$BOCHS_PATH")/.." && pwd -P)"
    search_dirs+=(
      "$prefix/lib/bochs/plugins"
      "$prefix/lib64/bochs/plugins"
      "$prefix/share/bochs/plugins"
    )
  fi

  for dir in "${search_dirs[@]}"; do
    [[ -d "$dir" ]] || continue
    for lib in "$dir"/libbx_*_gui.so; do
      [[ -f "$lib" ]] || continue
      local base=${lib##*/}
      base=${base#libbx_}
      base=${base%_gui.so}
      found["$base"]=1
    done
  done

  local prefs=(sdl2 sdl x wx term rfb nogui)
  for candidate in "${prefs[@]}"; do
    if [[ -n "${found[$candidate]:-}" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  for candidate in "${!found[@]}"; do
    printf '%s\n' "$candidate"
    return 0
  done

  return 1
}

detect_romimage() {
  local -a base_dirs=()
  local -a candidates=()

  if [[ -n "${BXSHARE:-}" ]]; then
    base_dirs+=("$BXSHARE")
  fi

  if [[ -n "${LTDL_LIBRARY_PATH:-}" ]]; then
    IFS=':' read -ra ltdl_dirs <<<"${LTDL_LIBRARY_PATH}"
    for dir in "${ltdl_dirs[@]}"; do
      [[ -n "$dir" ]] && base_dirs+=("$dir/..")
    done
  fi

  base_dirs+=(
    "/usr/share/bochs"
    "/usr/share/bochsbios"
    "/usr/local/share/bochs"
    "/usr/local/share/bochsbios"
  )

  if [[ -n "${BOCHS_PATH:-}" ]]; then
    local prefix
    prefix="$(cd "$(dirname "$BOCHS_PATH")/.." && pwd -P)"
    base_dirs+=(
      "$prefix/share/bochs"
      "$prefix/share/bochsbios"
    )
  fi

  local -a names=(
    "BIOS-bochs-latest"
    "BIOS-bochs-legacy"
    "BIOS-bochs-latest.bin"
    "BIOS-bochs-legacy.bin"
    "bios.bin"
    "BIOS-bochs-latest.rom"
    "BIOS-bochs-legacy.rom"
  )

  for dir in "${base_dirs[@]}"; do
    [[ -d "$dir" ]] || continue
    for name in "${names[@]}"; do
      local path="$dir/$name"
      if [[ -f "$path" ]]; then
        printf '%s\n' "$path"
        return 0
      fi
    done
  done

  return 1
}

detect_vgaromimage() {
  local -a base_dirs=()

  if [[ -n "${BXSHARE_PATH:-}" ]]; then
    base_dirs+=("$BXSHARE_PATH")
  fi
  if [[ -n "${BXSHARE:-}" && "${BXSHARE:-}" != "${BXSHARE_PATH:-}" ]]; then
    base_dirs+=("$BXSHARE")
  fi

  base_dirs+=(
    "/usr/share/bochs"
    "/usr/share/vgabios"
    "/usr/local/share/bochs"
    "/usr/local/share/vgabios"
  )

  local -a names=(
    "VGABIOS-lgpl-latest"
    "VGABIOS-lgpl-latest.bin"
    "VGABIOS-lgpl-latest.rom"
    "VGABIOS-bochs-latest"
    "VGABIOS-bochs-legacy"
    "VGABIOS-bochs-latest.bin"
    "VGABIOS-bochs-legacy.bin"
    "vgabios.bin"
  )

  for dir in "${base_dirs[@]}"; do
    [[ -d "$dir" ]] || continue
    for name in "${names[@]}"; do
      local path="$dir/$name"
      if [[ -f "$path" ]]; then
        printf '%s\n' "$path"
        return 0
      fi
    done
  done

  return 1
}

BOCHS_PATH="$(command -v "$BOCHS_BIN" 2>/dev/null || true)"
if [[ -z "$BOCHS_PATH" || ! -x "$BOCHS_PATH" ]]; then
  echo "Bochs binary '$BOCHS_BIN' not found. Install bochs or set BOCHS_BIN." >&2
  exit 1
fi

if [[ -z "$DISPLAY_LIBRARY" ]]; then
  if DISPLAY_LIBRARY="$(detect_display_library)"; then
    :
else
  echo "Warning: Unable to auto-detect a Bochs display plugin; using template default." >&2
fi
fi

if [[ -z "$ROMIMAGE_PATH" ]]; then
  if ! ROMIMAGE_PATH="$(detect_romimage)"; then
    cat >&2 <<'EOF'
Error: Could not locate a Bochs BIOS ROM image.
Install the 'bochsbios' package or set BOCHS_ROMIMAGE to the ROM path.
EOF
    exit 1
  fi
elif [[ ! -f "$ROMIMAGE_PATH" ]]; then
  echo "Error: BOCHS_ROMIMAGE '$ROMIMAGE_PATH' does not exist." >&2
  exit 1
fi

if [[ -z "$VGAROM_PATH" ]]; then
  if ! VGAROM_PATH="$(detect_vgaromimage)"; then
    cat >&2 <<'EOF'
Error: Could not locate a VGA BIOS ROM image.
Install the 'vgabios' package or set BOCHS_VGAROMIMAGE to the ROM path.
EOF
    exit 1
  fi
elif [[ ! -f "$VGAROM_PATH" ]]; then
  echo "Error: BOCHS_VGAROMIMAGE '$VGAROM_PATH' does not exist." >&2
  exit 1
fi

if [[ -z "$BXSHARE_PATH" ]]; then
  BXSHARE_PATH="$(dirname "$ROMIMAGE_PATH")"
fi
if [[ -z "$BXSHARE_PATH" && -n "$VGAROM_PATH" ]]; then
  BXSHARE_PATH="$(dirname "$VGAROM_PATH")"
fi

MEMORY="${1:-512}"
if [[ $# -gt 0 ]]; then
  shift
fi
EXTRA_ARGS=("$@")

TMP_CONFIG="$(mktemp)"
cleanup() {
  rm -f "$TMP_CONFIG"
}
trap cleanup EXIT

DISPLAY_LINE=""
if [[ -n "$DISPLAY_LIBRARY" ]]; then
  DISPLAY_LINE="$DISPLAY_LIBRARY"
  if [[ -n "$DISPLAY_OPTIONS" ]]; then
    opts="${DISPLAY_OPTIONS//\\/\\\\}"
    opts=${opts//\"/\\\"}
    DISPLAY_LINE+=", options=\"${opts}\""
  fi
fi

python3 - "$CONFIG_TEMPLATE" "$TMP_CONFIG" "$MEMORY" "$DISPLAY_LINE" "$ROMIMAGE_PATH" "$VGAROM_PATH" "$CPU_COUNT" <<'PY'
import sys
template, dest, mem, display, rom, vgarom, cpus = sys.argv[1:8]
with open(template, "r", encoding="utf-8") as src:
    lines = src.readlines()
with open(dest, "w", encoding="utf-8") as dst:
    for line in lines:
        if line.startswith("megs:"):
            dst.write(f"megs: {mem}\n")
        elif display and line.startswith("display_library:"):
            dst.write(f"display_library: {display}\n")
        elif rom and line.startswith("romimage:"):
            dst.write(f"romimage: file={rom}\n")
        elif vgarom and line.startswith("vgaromimage:"):
            dst.write(f"vgaromimage: file={vgarom}\n")
        elif cpus and line.startswith("cpu:"):
            dst.write(f"cpu: count={cpus}, ips=100000000, reset_on_triple_fault=1\n")
        else:
            dst.write(line)
PY

pushd "$ROOT_DIR" >/dev/null
if [[ "${SKIP_GRUB:-0}" != "1" ]]; then
  make grub
fi
BOCHS_ARGS=(-f "$TMP_CONFIG" -q)
CMD=("$BOCHS_BIN" "${BOCHS_ARGS[@]}" "${EXTRA_ARGS[@]}")
if [[ -n "$BXSHARE_PATH" ]]; then
  export BXSHARE="$BXSHARE_PATH"
fi
if [[ "$RUN_IMMEDIATE" == "1" ]]; then
  python3 - "$RUN_IMMEDIATE" "${CMD[@]}" <<'PY'
import os, pty, select, sys
run_immediate = sys.argv[1] == "1"
cmd = sys.argv[2:]
if not cmd:
    sys.exit("No command provided")

def auto_continue(buf):
    prompts = (b"<bochs:",)
    for prompt in prompts:
        if prompt in buf:
            return True
    return False

if not run_immediate:
    os.execvp(cmd[0], cmd)

pid, master_fd = pty.fork()
if pid == 0:
    os.execvp(cmd[0], cmd)

stdin_fd = sys.stdin.fileno()
stdout_fd = sys.stdout.fileno()
buffer = b""
try:
    while True:
        rlist, _, _ = select.select([master_fd, stdin_fd], [], [])
        if master_fd in rlist:
            try:
                data = os.read(master_fd, 1024)
            except OSError:
                data = b""
            if not data:
                break
            os.write(stdout_fd, data)
            buffer += data
            if auto_continue(buffer):
                os.write(master_fd, b"c\n")
                buffer = b""
            elif len(buffer) > 64:
                buffer = buffer[-64:]
        if stdin_fd in rlist:
            try:
                user = os.read(stdin_fd, 1024)
            except OSError:
                user = b""
            if not user:
                break
            os.write(master_fd, user)
finally:
    _, status = os.waitpid(pid, 0)
    if os.WIFEXITED(status):
        sys.exit(os.WEXITSTATUS(status))
    if os.WIFSIGNALED(status):
        sys.exit(128 + os.WTERMSIG(status))
    sys.exit(1)
PY
else
  "${CMD[@]}"
fi
popd >/dev/null
