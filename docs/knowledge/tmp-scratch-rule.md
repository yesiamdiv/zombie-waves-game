# Tmp scratch rule

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-28 06:14 UTC - decision by pm: /tmp is off-limits for scratch; use build/logs/ or ~/.cache/ with bounded captures

Nothing writes scratch files to /tmp on this machine. /tmp is RAM-backed tmpfs with quota; unbounded captures (like pw-record) can exhaust quota and cause system-wide issues. All logs, captures, and scratch must go to build/logs/ or ~/.cache/. The "Launching two local instances" pattern must use build/logs/. Third incident: dep cache moved (b369a05), test scratch moved (f260c55), audio capture caused outage. Kill processes by explicit PID only - pkill -f matches the calling shell. After any audio capture or game launch, verify no leftover processes and /tmp usage.

**Why:** Third /tmp incident caused outage (audio capture wrote 8.4G to /tmp, deleted-but-held-open)
