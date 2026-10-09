#!/usr/bin/env bash
# watch_agents.sh - block until the shared agents/ channel gets a new file or an
# appended reply, then print the new content and exit.
#
# agents/ is the only channel between agents here (AGENT_RULES.md 11). Replies
# are appends to topic files, not necessarily new files, so this watches for
# BOTH: a file that did not exist, and a file that grew. A file that exists but
# is EMPTY is not reported; we wait until it has content.
#
# Usage:
#   tools/watch_agents.sh                 wait for the next update (forever)
#   tools/watch_agents.sh --timeout 300   give up after 5 min (exit 2)
#   tools/watch_agents.sh --read          dump all current files and exit
#   tools/watch_agents.sh --loop          keep waiting and printing
#   tools/watch_agents.sh --dir DIR       watch DIR instead of <repo>/agents
#
# Exit: 0 = update printed, 2 = timed out, 3 = agents dir missing, 64 = bad arg.
# State (which bytes have been seen) lives in build/logs/ in the worktree, never
# in /tmp.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
agents_dir="$repo_root/agents"
timeout_s=0
mode="wait"

while [ $# -gt 0 ]; do
    case "$1" in
        --timeout) timeout_s="${2:?--timeout needs seconds}"; shift 2 ;;
        --dir)     agents_dir="${2:?--dir needs a path}";      shift 2 ;;
        --read)    mode="read"; shift ;;
        --loop)    mode="loop"; shift ;;
        -h|--help) sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "watch_agents: unknown arg: $1" >&2; exit 64 ;;
    esac
done

[ -d "$agents_dir" ] || { echo "watch_agents: no such dir: $agents_dir" >&2; exit 3; }

state_dir="$repo_root/build/logs"
mkdir -p "$state_dir"
state_tag="$(printf '%s' "$agents_dir" | md5sum | cut -c1-8)"
state="$state_dir/agents_watch.$state_tag.state"
touch "$state"

# "name size" for every regular file in the channel, one per line.
snapshot() {
    find "$agents_dir" -maxdepth 1 -type f -printf '%f %s\n' 2>/dev/null | sort
}

if [ "$mode" = "read" ]; then
    for f in "$agents_dir"/*; do
        [ -f "$f" ] || continue
        printf '=== %s ===\n' "$f"
        cat "$f"
        printf '\n'
    done
    exit 0
fi

# First run adopts the current files as the baseline, so we report the NEXT
# change, not everything that already exists.
if [ ! -s "$state" ]; then
    snapshot > "$state"
fi

# Print new/grown files' appended bytes. Returns 0 if anything was printed.
report_changes() {
    local newsnap="$1" printed=0 name size prev
    while IFS=' ' read -r name size; do
        [ -n "$name" ] || continue
        [ -f "$agents_dir/$name" ] || continue
        prev="$(awk -v n="$name" '$1==n {print $2}' "$state")"
        if [ -z "$prev" ]; then
            if [ "$size" -gt 0 ]; then
                printf '=== AGENTS UPDATE: NEW FILE %s (%s bytes) ===\n' "$name" "$size"
                cat "$agents_dir/$name"
                printf '\n=== END %s ===\n' "$name"
                printed=1
            fi
        elif [ "$size" -gt "$prev" ]; then
            printf '=== AGENTS UPDATE: APPEND to %s (+%s bytes) ===\n' "$name" "$((size - prev))"
            tail -c "+$((prev + 1))" "$agents_dir/$name"
            printf '\n=== END %s ===\n' "$name"
            printed=1
        fi
    done <<< "$newsnap"
    [ "$printed" -eq 1 ]
}

deadline=0
[ "$timeout_s" -gt 0 ] && deadline=$(( $(date +%s) + timeout_s ))

while true; do
    snap="$(snapshot)"
    if report_changes "$snap"; then
        printf '%s\n' "$snap" > "$state"
        [ "$mode" = "loop" ] || exit 0
    fi
    if [ "$deadline" -ne 0 ] && [ "$(date +%s)" -ge "$deadline" ]; then
        echo "watch_agents: no update after ${timeout_s}s" >&2
        exit 2
    fi
    sleep 2
done
