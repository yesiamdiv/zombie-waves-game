# intercom-agent-wake

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-11-16 08:24 UTC - note by pm: Intercom can wake Claude Code agents; "offline" means unsupervised, not un-wakeable

Correction from the owner (2026-11-16): Intercom CAN wake a Claude Code agent. The launcher runs a wake bridge that polls the agent's inbox every ~3s. So an agent shown as "offline" is not un-wakeable — "offline" only means nothing is supervising it.

Two real causes seen with the effects agent (it looked offline for 11d and a queued task did not start it):
1. Its member record had adapter='none', so the launcher skipped it entirely. Fixed to adapter='claude-code' plus its allow_commands list (it had none, so it could not run cmake/ctest at all).
2. Its first turn exceeded the launcher's 300s bootstrap timeout, which aborts BEFORE the wake bridge starts. Relaunched with a longer timeout.

Operating rule: when a Claude Code agent is not running, do NOT reassign its lane. Ask the owner to relaunch/resume it (the launcher/wake bridge will then pick up queued Intercom messages). Reassignment is the wrong default.
