# The control port

The TV asks the listener for bridges on its control port, 48054 by default:
one line in, one line out, then the connection closes. For remote play this
port is forwarded to the internet, which is why some commands are refused to
some callers and why a TV calling from the internet proves who it is.

## Commands

| Command | Answer | Who may send it |
|---|---|---|
| `STATUS` | `OK CTM_AGENT_V1` | anyone: it is the TV's probe |
| `PROOF_CHALLENGE` | `OK NONCE <32 hex>` | anyone: see "The pairing proof" |
| `BRIDGE_START <kind> <port> <busid> [PROOF=...]` | `OK bridge starting`; with port 0, `OK bridge starting port=<n>` | anyone on this PC or the home network; from the internet, a TV that proves it is paired (see below) |
| `BRIDGE_STOP <busid> [PROOF=...]` | `OK bridge stopped`, `OK no such bridge`, or `ERR that bridge belongs to another TV` | the same as `BRIDGE_START`, and only for its own bridges |
| `USBIP_ATTACH <busid>` | `OK usbip attached` | this PC only |
| `RESTART [hard]` | `OK bridges reset` | this PC only |

A refused command is answered `ERR` and a reason, and device.log has a
`control:` line saying who was refused and why. `USBIP_ATTACH` makes this PC
import a USB device from the caller's own USB/IP server, and `RESTART` drops
every bridge, so neither is the TV's to send.

## The pairing proof

A TV calling from the internet proves it is paired with this PC's streaming
host (Apollo or Sunshine) with the key it paired with:

1. It sends `PROOF_CHALLENGE` and gets a nonce: 32 lowercase hex, used once,
   by the address it was given to, within 30 seconds.
2. It sends its command with one more word on the end,
   `PROOF=<nonce>:<fingerprint>:<signature>`:
   - the fingerprint is the first 16 bytes, in lowercase hex, of the SHA-256
     of its certificate (DER);
   - the signature is RSA PKCS#1 v1.5 over SHA-256, in base64, of
     `DS5-USBIP proof 1`, a newline, the nonce, a newline, and the command
     exactly as sent without the proof.

The listener finds the certificate among the host's paired devices, in the
host's `sunshine_state.json` (`root.named_devices`), and checks the signature
with it.

## The bridge port is the caller's

Every `BRIDGE_START` holds its bridge port for the address that sent it, from
the home network as from the internet: a connection from anywhere else is
closed, and device.log says so (`bridge port <n>: refused a connection from
...`). The bridge ports are forwarded for remote play like the control port,
so a port that took anyone could be reached from the internet in the 30
seconds before its TV connects, or the 15 after it drops. No TV is turned
away by it, since a TV connects from the address it asked from. Only
`paired_tv_proof = off` lets a bridge port take anyone again. ⓘ A session
started over the REST API is held for nobody; REST is this PC's alone unless
it runs with `--rest-lan`.

## Who needs a proof

A TV on the home network (the private address ranges, Tailscale's included)
or on this PC needs no proof. One it sends anyway is checked and logged.

ⓘ An older listener answers `PROOF_CHALLENGE` with `ERR unknown command`, and
the TV then sends its command plainly; one that does not know `PROOF=` reads
the command's own words and ignores the last one. So a new TV works with an
old listener, and an old TV with a new one wherever no proof is needed.

## Two TVs at once

Two TVs can stream from this PC and bridge their controllers at the same
time. A bridge belongs to the TV that started it, known by the fingerprint its
pairing proof proved; a TV that sent no proof, or one that was not proven,
counts as one more "unproven" TV, as every TV did before.

- A TV asks for port 0 and the listener gives it a free port from
  `bridge_port_range`, in its answer: a TV at another site the LOWEST, the
  ports it always used and the router already forwards; a TV at home (or on
  this PC) the HIGHEST, so it never takes those first. An older TV chooses its
  own port, as before.
- A busid or a port already used by another TV's bridge is refused, never
  taken over. The TV's own older bridge on the port it asked for is replaced,
  as before.
- A TV stops only its own bridges, and a new bridge retires only the same TV's
  older one for the same pad.
- A device the TV sent no serial for gets one that also depends on the TV, so
  two TVs' pads of one model are two devices to Windows.

ⓘ A TV at another site reaches this PC at the home's public address, through
ports the router forwards: the control port, and from the bottom of
`bridge_port_range` one port per bridged controller and one for the TV's
remote. A TV at home needs no forwarding. Two TVs at other sites at once use
more of the low ports than one did, so the router has to forward that many.

## Settings

In `ctm-device-config.txt`, section `[global]`:

| Setting | Meaning |
|---|---|
| `paired_tv_proof = log` | the default for now: proofs are checked and logged, and no TV is turned away |
| `paired_tv_proof = on` | a TV calling from the internet without a good proof is refused |
| `paired_tv_proof = off` | nothing is checked, and a bridge port takes a connection from anywhere |
| `paired_tv_proof_at_home = true` | the home network must prove it too |
| `streaming_host_state_file = <path>` | where the host's state file is, when it is not Apollo's or Sunshine's in Program Files |
| `bridge_port_range = 48055-48074` | the ports the listener hands out for bridges, the default; at most 1000 of them |
