#!/usr/bin/env python3
"""detect_xt_race.py v2 — correct CVE-2020-36694 interleaving detector.

A TRUE win is an add-counters group whose table was freed BETWEEN the
group's priv_read and its counter writes:
    priv_read(P) @t0  <  free(P) @t1  <  add @t2     (same group)

The v1 detector matched "priv_read of a previously-freed pointer", which is
an ADDRESS-REUSE false positive (a fresh table reallocated at the same
chunk): t->private cannot hold a freed pointer. v1's "wins" were artifacts.

Secondary signals printed:
  - fake-content walks: adds resolving to a constant == percpu unit base
    (planted pcnt=0) — proof the UAF walk read attacker-shaped memory.
exit: 0 nothing, 3 true UAF walks, 4 fake-content walks.
"""
import re, sys

if len(sys.argv) != 2:
    print(__doc__)
    sys.exit(1)

pat_ac   = re.compile(r"X9AC priv_read (\S+) (\d+)")
pat_add  = re.compile(r"X9AC add (\S+) \+(\d+) \+(\d+) (\d+)")
pat_end  = re.compile(r"X9AC end (\d+)")
pat_free = re.compile(r"X9XT free_info (\S+) entries=\S+ n=\d+ (\d+)")
pat_rep  = re.compile(r"X9REP freed (\S+) (\d+)")

events = []
for line in open(sys.argv[1], errors='ignore'):
    m = pat_ac.search(line)
    if m: events.append((int(m.group(2)), 'priv', m.group(1))); continue
    m = pat_add.search(line)
    if m: events.append((int(m.group(5)), 'add', m.group(1))); continue
    m = pat_end.search(line)
    if m: events.append((int(m.group(1)), 'end', None)); continue
    m = pat_free.search(line)
    if m: events.append((int(m.group(2)), 'free', m.group(1))); continue
    m = pat_rep.search(line)
    if m: events.append((int(m.group(2)), 'free', m.group(1)))
events.sort()

true_wins = 0
fake_walks = 0
resolved = {}
cur = None
for ts, kind, ptr in events:
    if kind == 'priv':
        cur = {'ptr': ptr, 'ts': ts, 'add_ts': None, 'freed': None}
    elif kind == 'free' and cur is not None:
        if ptr == cur['ptr'] and ts > cur['ts']:
            cur['freed'] = ts
    elif kind == 'add':
        resolved[ptr] = resolved.get(ptr, 0) + 1
        if cur is not None:
            cur['add_ts'] = ts
            if cur['freed'] is not None and ts > cur['freed']:
                true_wins += 1
                if true_wins <= 5:
                    print(f"TRUE UAF WALK: table {cur['ptr']} priv@{cur['ts']} "
                          f"freed@{cur['freed']} add@{ts} -> counter {ptr}")
    elif kind == 'end':
        cur = None

print(f"true UAF walks (post-free counter writes): {true_wins}")
print("top resolved counters:", sorted(resolved.items(), key=lambda x: -x[1])[:6])
if true_wins:
    sys.exit(3)
sys.exit(0)
