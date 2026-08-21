import re, sys, os, collections

DEFAULT = r"D:\FortniteBuilds\ch5\++Fortnite+Release-31.41-CL-37324991-Windows\FortniteGame\Binaries\Win64\Boron_Console.txt"

def main(path):
    if not os.path.exists(path):
        print("no log at %s" % path); return 1
    txt = open(path, encoding="utf-8", errors="ignore").read()
    lines = txt.splitlines()
    print("=" * 68)
    print("Boron session report -- %s" % os.path.basename(path))
    print("  %d lines, %.1f KB" % (len(lines), os.path.getsize(path) / 1024.0))
    print("=" * 68)

    print("\n-- STALLS (the ping/hitch metric) --")
    flush = [int(m) for m in re.findall(r"FlushAsyncLoading\((\d+)", txt)]
    if flush:
        lo, hi = min(flush), max(flush)
        print("  FlushAsyncLoading counter %d -> %d = %d blocking sync loads" % (lo, hi, hi - lo))
        print("  (compare this delta across runs; lower is better)")
    else:
        print("  no FlushAsyncLoading counters found")

    guards = re.findall(r"LIGHTWEIGHT_TIME_GUARD: (?:FTickFunctionTask - )?(\S+).*?took ([\d.]+)ms", txt)
    if guards:
        worst = collections.defaultdict(float)
        for name, ms in guards:
            worst[name.split("/")[0]] = max(worst[name.split("/")[0]], float(ms))
        print("  worst single stalls:")
        for name, ms in sorted(worst.items(), key=lambda kv: -kv[1])[:6]:
            print("     %8.1f ms  %s" % (ms, name[:52]))

    print("\n-- BORON STATE --")
    checks = [
        ("Iris: FortWeapon routed",  r"CLEARED filter on /Script/FortniteGame\.FortWeapon"),
        ("Server became Joinable",   r"UE [\d.]+\): Joinable|gsStatus"),
        ("Native mod apply",         r"native TryAddWeaponMod available=(\d)"),
        ("Native pickup mods",       r"native ApplyWeaponModToPickup available=(\d)"),
        ("Inventory desync seen",    r"Remove DESYNC"),
        ("Full-inv swap seen",       r"full-inv swap: dropping"),
        ("Log budget exhausted",     r"\[LogCap\]"),
    ]
    for label, pat in checks:
        m = re.search(pat, txt)
        print("  %-26s %s" % (label, ("YES  " + (m.group(0)[:44] if m else "")) if m else "no"))

    for label, pat in [("aircraft dispatch", r"dispatch: FlightInfos=\d+ lategame=\d+ zones=\d+[^\n]*"),
                       ("mods discovered",   r"discovered \d+ weapon mod definitions"),
                       ("iris filters",      r"filters: configs=\d+ clearedInventory=\d+ clearedPickupLike=\d+")]:
        m = re.search(pat, txt)
        if m: print("  %-26s %s" % (label, m.group(0)))

    print("\n-- BORON TAGS THAT FIRED --")
    tags = collections.Counter(re.findall(r"\[Boron\]\[([A-Za-z0-9_]+)\]", txt))
    for t, n in sorted(tags.items()):
        print("  %-14s x%d" % (t, n))

    print("\n-- ENGINE NOISE (top categories) --")
    cats = collections.Counter(m for m in re.findall(r"^(Log[A-Za-z]+): (?:Error|Warning)", txt, re.M))
    for c, n in cats.most_common(8):
        print("  %-34s %d" % (c, n))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else DEFAULT))
