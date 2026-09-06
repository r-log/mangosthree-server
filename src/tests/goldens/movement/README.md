# Movement wire goldens

Captures of real 4.3.4 (build 15595) movement packets, one per line in the
server's `Movement.CaptureFile` format:

    <direction> 0x<opcode> <payload hex>

`C` is client to server, `S` is server to client; `#` starts a comment.
`GoldenCaptureTest` replays the goldens listed below through the wire registry
(`src/proto/wire/MovementLayouts.inc`): each registered line must decode
whole and re-encode to the same bytes. The files are named in the test source,
not discovered, so a golden that goes missing fails instead of vanishing --
adding one here means adding it there too. A change to the codec or a table
that breaks a replay fails the suite, which is the point.

`client-15595.log` is trimmed (four high-volume opcodes cut to their first 40
and last 10 lines, see the table), and
`GoldenCapture_client_fall_blocks_settle_the_fall_angle_labels` walks it in
order with a stateful jump/fall window: re-trimming it changes which packets
that window sees, so re-run that test after any re-trim.

| File | Provenance |
|---|---|
| `peer-walk-15595.log` | The synthetic peer (`mangos-loadtest --walk 6 --return --pair`) against this server, master 59e3b47cb, 2026-09-06: two sessions logging in (three speed sets and their acks apiece -- run, swim, flight), a walk out and back with the second session observing it (26 `SMSG_PLAYER_MOVE` relays), and three creature spline-speed broadcasts that happened to be in range. Every line was built by this tree's own codec, so it proves the round trip, not the layouts. |
| `client-15595.log` | A real 15595 client's session against this server (branch feat/movement-wire-parity), 2026-09-06: standing, walking forward and backward, strafing, turning by keyboard and mouse, walk mode toggled, jumps in several headings, swimming with pitch, a ground mount and a flying mount (mount casts, speed sets and acks, can-fly set/unset and acks, collision-height sets), and a GM `.freeze`/`.unfreeze` (its root/unroot and both acks came from that command, not a PvP root). No transport ride. `MSG_MOVE_HEARTBEAT`, `CMSG_MOVE_SET_FACING`, `SMSG_SPLINE_MOVE_SET_RUN_MODE` and `SMSG_SPLINE_MOVE_SET_WALK_MODE` are trimmed to the first 40 and last 10 lines each (the raw capture had hundreds to over a thousand of each); everything else is kept, in order. These lines were built by the client, so they are the layouts' ground truth; the four server-built `SMSG_MOVE_SET_COLLISION_HGT` lines (sent at the two mount/dismount cycles) are a known exception -- the legacy writer (`src/game/Object/Unit.cpp`) still emits the WotLK shape, not the 4.3.4 one, and `GoldenCapture_server_built_lines_fail_only_where_the_legacy_writer_is_known_wrong` pins exactly those four as the expected failures until P2 rewrites that writer. |
