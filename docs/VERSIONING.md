# VERSIONING.md — the project's version-numbering standard

**Status: MANDATORY. Read this before adding a system, a wire field, an asset, or
a map format.**

This repo has accumulated versions the hard way: each one was invented in the
middle of a bug, usually *after* it had already broken a client. This document
exists so the next version is a deliberate act instead of an accident.

Companion: `docs/SYSTEM_DESIGN_RULES.md` (how to design so you don't break other
systems).

---

## 1. Why versions are not optional

Every version number in this project guards exactly one thing: **two things that
cannot understand each other must be able to notice and refuse.**

Without that check, a mismatch does not crash. It renders a plausible wrong
world. That is the failure mode this repo keeps hitting:

| What broke | How it presented |
|---|---|
| HELLO layout change (v2→v3) | a v2 peer read the map byte as the first letter of the host name |
| `NetEntitySnap` gaining fields (v3→v4) | a v3 peer would stride 26 bytes over 31-byte entries and desync on entity one |
| Missing textures after the assets merge (B15–B18) | flat circles, no error, no message |
| Client drawing its own health/inventory (B22, B23) | plausible-looking wrong numbers |

Not one of these announced itself. That is the argument for the rule.

## 2. The registry — one number, one file, one meaning

**Every version in the project is declared exactly once.** If a second copy of a
number exists anywhere, it is a bug. Do not "just update the other one too" —
find out why it was duplicated.

| Version | Declared in | Current | Guards | Bump when |
|---|---|---|---|---|
| `NET_WIRE_VERSION` | `src/net/net.h` | **4** | Byte layout of every net packet body | Any field is added/removed/resized/ reordered in any packet body |
| `NET_WORLD_GEN_VERSION` | `src/net/net.h` | **2** | Two peers derive the *same world* | Map format, world generation, theme selection, or spawn rules change |
| `NET_ASSET_VERSION` | `src/net/net.h` | *(planned, sprint N2)* | Two peers ship the *same art* | Any file in `assets/` is added, removed, resized, or replaced |
| `MAP_FORMAT_VERSION` | map loader | *(planned)* | A `.map` file means the same thing | The map grammar changes at all |
| `GAME_VERSION` | `CMakeLists.txt` | *(planned)* | Humans talking to humans | Release-level, not compatibility |

Two rules that follow from the table:

- **Never reuse a number for a second meaning.** A number retired from the wire
  is dead, not free.
- **One concern, one version.** `NET_WIRE_VERSION` is not a
  catch-all. If art changes, `NET_ASSET_VERSION` is the one that moves, and
  that is what lets the error message say *why* two peers cannot talk.

## 3. Naming convention

```
NET_WIRE_VERSION        protocol   + _WIRE
NET_WORLD_GEN_VERSION   protocol   + _WORLD_GEN
NET_ASSET_VERSION       protocol   + _ASSET
MAP_FORMAT_VERSION      content    + _FORMAT
GAME_VERSION            project    + GAME
```

`<DOMAIN>_<ASPECT>_VERSION`. Domains in use: `NET`, `MAP`, `GAME`. Aspects name
the concern, never the change. A name like `NET_WIRE_VERSION_2` is wrong: the
number lives in the value, not the name.

## 4. Bump policy

Whole positive integers. No semantic-version strings on the wire — a peer
comparing strings for equality is a peer that can be fooled by trailing spaces.

**Bump when a change is not backward compatible.** Concretely:

| Change | Bump? | Why |
|---|---|---|
| Add a field to a packet body | **yes** | the old reader strides the wrong distance |
| Remove or reorder a field | **yes** | same, immediately |
| Resize a field | **yes** | silent corruption |
| Change what an existing field *means* | **yes** | both sides "succeed" and disagree |
| Add a new packet kind | no | old peers never send it; unknown kinds are dropped |
| Add a new enum value that older peers may not see | **yes**, if a peer could receive it | older peers cannot resolve it |
| Change local-only game logic | no | not cross-process state |
| Add an asset | **yes, `NET_ASSET_VERSION`** | peers would render different pictures |
| Widen a value that already fits | no | old readers already handled the range |

**Two peers must match exactly.** There is no "close enough" and no minor-version
negotiation. A v4 host does not try to accommodate a v3 client; it refuses it.
This is deliberate: the alternative is a compatibility layer for a protocol with
two real implementations, which is how subtle desyncs are manufactured.

## 5. The agent's rules

1. **Before you edit a version-guarded structure, find its version.** If you
   cannot name which version your change invalidates, you do not yet understand
   the change. Stop and work that out.
2. **A commit that changes a wire layout and does not bump the version is an
   incomplete commit.** Same for a commit that changes a version without saying
   in the message *which* version and *why*.
3. **Bump and change in the same commit.** A commit that bumps a version with no
   behavioural change, or changes behaviour with no bump, leaves a broken state
   at that SHA — and this project's history already has four such commits
   (`eda5cd9`, `70ed1e0`, `8824f73`, `c22d008`).
4. **Update the codec test in the same commit.** `test_net_codec` asserts wire
   sizes; a new field without a new assertion is an untested layout.
5. **Add a comment at the declaration recording the bump history** — previous
   value, what changed, and what a stale peer would do. The three existing
   version macros all do this. Keep it accurate or delete it.
6. **A rejected connection must be explained to a human.** Bumping a version is
   pointless if the player is silently returned to the menu with the reason
   buried in a log file nobody opens.
7. **Never edit a version number to "make a test pass."** If a peer is being
   rejected, the correct outcome is to find out which of the two is wrong.

## 6. Worked example — the v3 → v4 bump, for calibration

The change: `NetEntitySnap` gained `art`, `size_q`, `tint[3]` so the client would
stop guessing what entities look like (R13-I5).

```
src/net/net.h          NET_WIRE_VERSION 3 -> 4, entry bytes 26 -> 31
src/net/net.h          struct NetEntitySnap + art, size_q, tint[3]
src/net/net_codec.c    encode/decode the five new bytes, appended after `owner`
src/net/net_snapshot.c host fills them from the entity's live CSprite
src/systems/render.c   client reproduces them; per-kind size table DELETED
tests                  appearance round-trip in test_net_codec
```

Five touches in five files, one commit. The comment at the macro records that a
v3 peer strides 26 bytes over 31-byte entries — which is the sentence that will
save whoever hits this in six months.

## 7. Before you open the pull request

- [ ] Every version my change invalidates has been identified **by name**
- [ ] Each one bumped, in the same commit as the change
- [ ] Codec/format test updated to assert the new layout
- [ ] The declaration's history comment updated
- [ ] Commit message names the version(s) and the reason
- [ ] The mismatch path is *tested*: two deliberately mismatched peers are
      refused, with a distinct reason code