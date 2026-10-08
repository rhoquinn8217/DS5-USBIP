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
| `BRIDGE_START <kind> <port> <busid> [PROOF=...]` | `OK bridge starting` | anyone on this PC or the home network; from the internet, a TV that proves it is paired (see below) |
| `BRIDGE_STOP <busid> [PROOF=...]` | `OK bridge stopped`, or `OK no such bridge` | the same as `BRIDGE_START` |
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
with it. A proven `BRIDGE_START` from the internet also holds its bridge port
for that caller's address: a connection from anywhere else is closed.

A TV on the home network (the private address ranges, Tailscale's included)
or on this PC needs no proof. One it sends anyway is checked and logged.

ⓘ An older listener answers `PROOF_CHALLENGE` with `ERR unknown command`, and
the TV then sends its command plainly; one that does not know `PROOF=` reads
the command's own words and ignores the last one. So a new TV works with an
old listener, and an old TV with a new one wherever no proof is needed.

## Settings

In `ctm-device-config.txt`, section `[global]`:

| Setting | Meaning |
|---|---|
| `paired_tv_proof = log` | the default for now: proofs are checked and logged, and nobody is turned away |
| `paired_tv_proof = on` | a TV calling from the internet without a good proof is refused |
| `paired_tv_proof = off` | nothing is checked |
| `paired_tv_proof_at_home = true` | the home network must prove it too |
| `streaming_host_state_file = <path>` | where the host's state file is, when it is not Apollo's or Sunshine's in Program Files |
