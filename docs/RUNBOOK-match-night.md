# Runbook: match night (M1)

The verified path for hosting a co-op Foundry match over Tailscale.

## 1. Verify before you invite anyone

```sh
cd ~/dev/deepfield-3d
make check                      # gates green or don't host
./play                          # window opens → click HOST: the lobby becomes your party
```

**HOST opens a party, not a match.** The world exists and holds in the lobby:
the seats row fills as friends join, everyone picks a faction against each
other's (a taken one says so before the sim has to refuse it), and the match
starts when you press **LAUNCH**. Toggle **ENDLESS** in the strip before
hosting for an open-ended run. The header shows the invite address the moment
you host.

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

Send friends the ip from `/info` (or from the lobby header): they enter
`100.x.x.x` (or `ip:port`) in the JOIN field and land in the same lobby, with
"waiting for the host to launch" in the strip. Each player picks a different
faction — a column another player holds is marked `taken · name`, and the sim
refuses duplicates anyway ("factionTaken" means pick another one). A dedicated
server (`--server`) has no lobby: joiners drop straight in at the next
intermission, as before.

Someone joining a match already under way skips the lobby and drops in at the
next intermission.

## 3. Play

- **Hold E** at a socket opens the build wheel — mouse steers, release builds.
  The wheel only offers what that socket accepts and greys out what you can't
  afford. **Hold U** at a placed structure to level a path (1–3) or **hold X**
  to sell.
- **Scrap from flyers falls.** A Skiff dies over the lane and its scrap comes
  down to the floor under it rather than hanging where it died, so the drop you
  earned is one you can reach.
- **Walk over the scrap.** A kill banks the team's half instantly and drops
  your half on the floor; it drifts to you when you get close, and what nobody
  collects goes to the team pool after 45 seconds. Weapon and melee platforms
  are bought with that personal scrap, so the shared wallet stays the towers'.
- **F** starts a wave early, **Q** is your faction ability, **hold R** revives a
  downed teammate (their beacon shows through walls), **Tab** opens the armory
  and gunsmith, **Esc** is the menu.
- Traversal: ladders (W climbs), the launcher pad flings you onto the deck, and
  **E** inside a zipline volume rides it down to the core gate.

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
