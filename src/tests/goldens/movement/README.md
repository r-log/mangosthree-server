# Movement wire goldens

Captures of real 4.3.4 (build 15595) movement packets, one per line in the
server's `Movement.CaptureFile` format:

    <direction> 0x<opcode> <payload hex>

`C` is client to server, `S` is server to client; `#` starts a comment.
`GoldenCaptureTest` replays every `*.log` here through the wire registry
(`src/proto/wire/MovementLayouts.inc`): each registered line must decode
whole and re-encode to the same bytes. A change to the codec or a table that
breaks that fails the suite, which is the point.

| File | Provenance |
|---|---|
| `peer-walk-15595.log` | The synthetic peer (`mangos-loadtest --walk 6 --return --pair`) against this server, master 59e3b47cb, 2026-09-06: two sessions logging in (three speed sets and their acks apiece -- run, swim, flight), a walk out and back with the second session observing it (26 `SMSG_PLAYER_MOVE` relays), and three creature spline-speed broadcasts that happened to be in range. Every line was built by this tree's own codec, so it proves the round trip, not the layouts. |
| `client-15595.log` | A real 15595 client's session against this server (P1-B Task 6): the maintainer's checklist run. These lines were built by the client, so they are the layouts' ground truth on the client side. |
