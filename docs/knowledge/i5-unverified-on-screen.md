# I5 and B32-B35 are fixed in code and unproven on screen

<!-- Project knowledge (Intercom-AI). Agents append entries; edit freely. -->

## 2026-10-09 12:35 UTC - note by net: Headless gates are structurally blind to this. Phase D is hard-blocked on a human two-window check.

HONEST STATE: I5 and B32-B35 are fixed in code, gates green, and **zero of them
are proven on screen**. `docs/PENDING_VERIFICATION.md` holds the two-window
checklist.

WHY NOT VERIFIED - environmental, not skipped. On this box there is no
`grim`/`gnome-screenshot`/`xdotool`/imagemagick; the GNOME screenshot portal
returns response `2` (and hangs for a human with `interactive=true`);
`org.gnome.Shell.Screenshot` is `AccessDenied`; and XWayland's root is not the
composited screen so region grabs come back black. `ffmpeg -f x11grab
-window_id <xid>` does capture, but an agent cannot *view* an image.

WHAT WAS PROVEN, NUMERICALLY: on a captured host frame the zombie counter
occupies y79-91 and the name tag y104-116 - a 13px gap - so B34's overlap is
gone and the host's own tag renders in slot-0 blue. That is numbers, not a
screenshot, and it says nothing about B32's client death screen or B35's
client-side colour.

CONSEQUENCE, AGREED BY BOTH AGENTS: animation Phase D (client-side animation)
is a **hard block** until a human has actually watched two windows.
`ANIMATION_PLAN.md` §5 says Phase D does not open on a green gate alone.
Designing client-side animation on a mirror path nobody has ever seen run is
the exact pattern that keeps burning this project.

GENERAL RULE THIS IS AN INSTANCE OF: `--headless` returns immediately from
`render()`, so no headless gate can see a visual defect. Write `unproven`, not
`fixed`, until a human has looked or listened.
