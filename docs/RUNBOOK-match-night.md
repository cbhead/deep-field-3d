# Runbook: match night (M1)

The verified path for hosting a co-op Foundry match over Tailscale.

## 1. Verify before you invite anyone

```sh
cd ~/dev/deepfield-3d
make check                      # gates green or don't host
./play                          # window opens → click HOST
```

Or dedicated (no window, e.g. from Mission Control or a spare machine):

```sh
./play --headless -- --server               # UDP+TCP 8787
./play --headless -- --server --port 8791   # alternate port
```

Then the curl-before-invite step, same as the 2D game:

```sh
curl -s http://localhost:8787/info
# {"app":"deepfield-3d","map":"foundry","players":0,"wave":0,"phase":"Intermission",
#  "tailscaleIp":"100.x.x.x","port":8787,"version":1}
```

- `app` present and `deepfield-3d` → it's this game holding the port, not the 2D relay
  (Mission Control keys on exactly this field).
- `tailscaleIp` empty → Tailscale is off; friends can't reach you. `tailscale up` first.
- An empty server idles in intermission — it will NOT burn waves before anyone joins.

## 2. Invite

Send friends the ip from `/info`: they enter `100.x.x.x` (or `ip:port`) in the
JOIN field. Each player picks a different faction — the sim refuses duplicates
("factionTaken" means pick the other one).

## 3. Play

- **E** on a socket builds (wall sockets auto-pick Skywatch), **U** upgrades the
  damage path, **F** starts the wave early, **Q** faction ability, **R** (held)
  revives, keys **1–4** buy/swap weapons at the armory block.
- Traversal: ladder on the deck's south face (W climbs), purple pad launches
  onto the deck, **E** in the zipline volume rides it down to the core gate.

## 4. Teardown

Ctrl-C the server (or close the host window). No persistent state; every match
is fresh. Rooms don't outlive the process.

## Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| "version mismatch" on join | client/server release skew | both update to the latest release |
| "connection failed" | wrong ip, Tailscale down, or port blocked | re-curl `/info`, check `tailscale status` |
| "factionTaken" | duplicate faction pick | pick the other faction |
| joined but frozen enemies | snapshot channel not flowing (check server log) | rejoin; file it if it repeats |
| `/info` answers but wrong game | the 2D relay owns the port | Mission Control probe, stop the other process |
