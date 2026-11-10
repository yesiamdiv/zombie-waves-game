#!/usr/bin/env python3
import argparse, os, subprocess, sys, time, signal, re
from pathlib import Path

def main():
    p = argparse.ArgumentParser(description="One-command headless co-op check (host+client)")
    p.add_argument("--host-port", type=int, default=5140)
    p.add_argument("--run-seconds-host", type=float, default=12)
    p.add_argument("--run-seconds-client", type=float, default=8)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--keep", action="store_true")
    p.add_argument("--bin", default=None)
    args = p.parse_args()
    repo = Path(__file__).resolve().parents[1]
    binp = Path(args.bin) if args.bin else repo / "build" / "open_world_zombie_waves"
    if not binp.exists():
        print(f"missing binary: {binp}", file=sys.stderr)
        return 2
    tmp = repo / ".tmp_coop_check"
    tmp.mkdir(parents=True, exist_ok=True)
    import socket
    s = socket.socket(); s.bind(('',0)); port = s.getsockname()[1]; s.close()
    host_log = tmp / "host.out"; client_log = tmp / "client.out"
    host_ev = tmp / "host.events"; client_ev = tmp / "client.events"
    import shutil, atexit
    procs = []
    def killall():
        for pr in procs:
            try:
                if pr.poll() is None:
                    os.killpg(os.getpgid(pr.pid), signal.SIGTERM)
            except Exception:
                pass
    atexit.register(killall)
    host = subprocess.Popen([str(binp), "--headless", "--host", f"--port={port}", "--name=Host", "--auto-start", f"--run-seconds={args.run_seconds_host}", f"--events={host_ev}", f"--seed={args.seed}"], stdout=open(host_log,'w'), stderr=subprocess.STDOUT, preexec_fn=os.setsid, cwd=repo)
    procs.append(host)
    time.sleep(2.0)
    client = subprocess.Popen([str(binp), "--headless", f"--join=127.0.0.1:{port}", "--name=Guest", "--ai=bot", "--auto-start", f"--run-seconds={args.run_seconds_client}", f"--events={client_ev}", f"--seed={args.seed}"], stdout=open(client_log,'w'), stderr=subprocess.STDOUT, preexec_fn=os.setsid, cwd=repo)
    procs.append(client)
    try:
        client.wait(timeout=args.run_seconds_client + 15)
    except subprocess.TimeoutExpired:
        pass
    try:
        host.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    killall()
    time.sleep(0.5)
    ok = 0
    def grep(path, pat):
        if not path.exists(): return []
        r=[]; 
        for l in open(path):
            if re.search(pat,l): r.append(l)
        return r
    h_spawn = grep(host_log, r"Spawned remote player entity")
    c_mirror = grep(client_log, r"CLIENT mirror")
    c_wave = grep(client_log, r"WAVE .*START")
    h_err = grep(host_log, r"ERROR|REJECT"); c_err = grep(client_log, r"ERROR|REJECT")
    # Also accept wave number appearing in client mirror logs
    if len(c_wave)==0:
        import re as _re
        for l in open(client_log):
            if _re.search(r"wave=[1-9]", l): c_wave=[l]; break
    print(f"host spawn: {len(h_spawn)}"); print(f"client mirror lines: {len(c_mirror)}"); print(f"client WAVE_START: {len(c_wave)}"); print(f"errors: host={len(h_err)} client={len(c_err)}")
    if len(h_spawn)==0 or len(c_mirror)==0 or len(c_wave)==0 or len(h_err)!=0 or len(c_err)!=0:
        print("FAIL", file=sys.stderr)
        sys.exit(1)
    print("PASS")
    if not args.keep:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0

if __name__=='__main__':
    raise SystemExit(main())
