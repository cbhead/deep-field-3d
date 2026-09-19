---
ws: 11
slug: online
title: Online, lobby, profile (EOS)
state: active
owner: session-75b58b1b/agent-ws11
claimed_at: 2026-09-19T15:16:34Z
lease_expires: 2026-09-20T15:16:34Z
branch: ws/11-online/ossv2-spike
last_commit: 
editor_heavy: false
phase: P1-P3
size: L
critical: true
blocked_on: 
---
# WS-11 — Online, lobby, profile (EOS)

## Scope / DoD
**Scope.** OSSv2 spike → UDFOnlineSubsystem; EOS login (EGS + dev auth), invite-only lobby, friend invite + join code with host approval, P2P relay NetDriver, join-in-progress, kick/session ban, version + content-hash handshake, autosave between waves + resume under a new host, UDFProfileSave + migrations, connection state machine, L2/L3 seams, Title Storage remote config.

**Definition of done.** Two machines join over relay; 150 ms join-in-progress; DF.Net.JoinLeaveRejoin; ResumeUnderNewHost.

**Spec.** C13–C14, ADR-0003, B§5 (in `unreal/PLAN/PROGRAMME.md`). Size L, phase P1-P3.

## Contracts I consume
- C1
- C12
- C15

## Contracts / interfaces I provide
- C13
- C14

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Session log
<!-- append-only: date · session · what landed · what's next -->
