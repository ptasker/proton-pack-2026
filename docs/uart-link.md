# Wand-to-pack UART link

The wand tells the pack what its switches and fire button are doing; the pack
follows. The link is enabled by `PACK_LINK_ENABLED` in
[WandConfig.h](../include/WandConfig.h) and `WAND_LINK_ENABLED` in
[Config.h](../include/Config.h). The cable is described in
[hardware.md](hardware.md).

This is a small custom protocol. It is not GPStar's serial protocol, and these
boards cannot be connected to GPStar firmware.

## Behaviour

- The pack boots off and waits for valid wand data.
- Activate on starts the pack; Activate off shuts it down.
- The pack fires when the wand reports firing, and enters cooldown when the
  wand does. It can still refuse to fire for its own safety checks.
- No new, valid status for 500 ms stops firing and shuts the pack down. Packets
  with a bad CRC, wrong version or repeated sequence number do not count.
- After start-up, reconnection or cooldown, the pack needs to see fire released
  before it accepts a new press. If the pack restarts while the wand is on,
  release fire and press again.
- With the link enabled, the pack's own power, fire and vent inputs must stay
  unassigned, and its `p`, `f`, `s`, `v` serial keys are rejected.

## Packet format

UART1 on each board, 9600 baud, 8N1. Every packet is eight bytes:

| Byte | Content |
| --- | --- |
| 0-1 | `A5 5A` |
| 2 | Protocol version, currently `2` |
| 3 | Type: 1 = wand status, 2 = pack acknowledgement, 3 = pack status |
| 4 | Sequence number |
| 5 | Flags (wand status and acknowledgement) |
| 6 | Speed level 1-4; must be 1 when not firing |
| 7 | CRC-8 over bytes 0-6, polynomial `0x07`, initial value 0 |

Flag bits:

| Bit | Meaning |
| --- | --- |
| 0 | Activate |
| 1 | Vent |
| 2 | Wand switch |
| 3 | Fire held |
| 4 | Starting |
| 5 | Firing |
| 6 | Cooling |
| 7 | Sound muted |

## Timing

- The wand sends a status on every change (at most one per 20 ms) and repeats
  it every 100 ms.
- The pack echoes each status as an acknowledgement. This confirms receipt, not
  that the pack accepted a firing request. Missing acknowledgements are
  reported on the wand but do not affect its lights.
- The pack sends a pack-status packet carrying the theme on connect, on change
  and every 500 ms.
- A partial packet is discarded after 50 ms.

Both boards must run the same protocol version, so update them together.

## Turning it off

Set the pack's `WAND_LINK_ENABLED = false` to run it standalone (see
[pack.md](pack.md)). Set the wand's `PACK_LINK_ENABLED = false` to stop its
UART traffic.
