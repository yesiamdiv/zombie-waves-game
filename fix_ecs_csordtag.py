import re

R = "/home/divyam-redkar/development/personal/SDL3/open-world-zombie-waves-multiplayer"
p = R + "/src/ecs/ecs.h"
s = open(p, encoding="utf-8").read()

canonical = (
    "typedef struct {\n"
    "    Entity owner;\n"
    "    float radius;\n"
    "    float outer_radius;\n"
    "    float angle;\n"
    "    float spin_speed;\n"
    "    float damage;\n"
    "    float hit_interval;  /* ours: configured re-hit interval for the sword sweep (MP kill-credit owner-2-entity) */\n"
    "    float hit_timer;  /* ours: running per-zombie re-hit cooldown (sword sweep) */\n"
    "    float sword_hit_timer;  /* main: running per-zombie re-hit cooldown (sword sweep) */\n"
    "    Entity last_hit_by;  /* ours+main union: owner credited with this zombie's kill (used by system_cleanup) */\n"
    "} CSwordTag;\n"
)

pat = re.compile(r"typedef struct \{.*?\} CSwordTag;", re.S)
m = pat.search(s)
assert m, "CSwordTag struct not found"
print("found CSwordTag, replacing region len=%d" % (m.end() - m.start()))
s = s[:m.start()] + canonical + s[m.end():]
assert s.count("CSwordTag;") == 1, "CSwordTag count=%d" % s.count("CSwordTag;")
for token in ("float hit_timer;", "float hit_interval;", "float sword_hit_timer;", "last_hit_by;"):
    assert token in s, "missing " + token
open(p, "w", encoding="utf-8").write(s)
print("=== AFTER: byte-true region ===")
j = s.index("typedef struct {\n    Entity owner;\n    float radius;\n")
j2 = s.index("CSwordTag;", j) + len("CSwordTag;")
print(s[j:j2])
print("=== DISK bytes ===")
print(repr(open(p, encoding="utf-8").read()[j:j2]))
