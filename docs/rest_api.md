# Agent REST API

Optional HTTP/JSON API for the agent, enabled with `--rest <port>`. It is a
*sibling* of the plaintext control channel on the agent port (the `STATUS` /
`BRIDGE_START` / `BRIDGE_STOP` / `RESTART` line protocol the TV speaks). The TV
keeps using that channel; REST serves the settings page and tooling: curl,
scripts, health checks.

```powershell
ctm-usbip agent --rest 48055                      # loopback-only, no auth
ctm-usbip agent --rest 48055 --rest-lan --rest-token <token>
ctm-usbip install --rest 48055                    # service mode carries the same flags
```

Off unless `--rest` is given, or `--ui`, which needs the settings page and so
turns REST on at 48055 when no port was named. If the REST port cannot be
bound the agent fails startup (exit 4) rather than running with a silently
missing API.

## Endpoints

Answers are `application/json` and `Connection: close`, except the page and
its icon.

### The settings page

| Method | Path | What |
|---|---|---|
| `GET` | `/`, `/index.html` | the settings page (HTML) |
| `GET` | `/favicon.ico` | the settings window's icon |

### Status and bridge sessions

| Method | Path | What |
|---|---|---|
| `GET` | `/api/v1/status` | agent and product version, transport, ports, uptime, session count |
| `GET` | `/api/v1/sessions` | all bridge sessions |
| `POST` | `/api/v1/sessions` | start a bridge session (like `BRIDGE_START`) |
| `GET` | `/api/v1/sessions/{busid}` | one session, or 404 |
| `DELETE` | `/api/v1/sessions/{busid}` | stop a session (like `BRIDGE_STOP`) |
| `POST` | `/api/v1/restart` | soft reset of all bridges, or hard service restart |

### Devices and configs

| Method | Path | What |
|---|---|---|
| `GET` | `/api/v1/devices` | every bridged device, with the config it uses |
| `POST` | `/api/v1/devices/{ordinal}/link` | link a config, `{"config":"<name>"}`; `{}` unlinks |
| `GET` | `/api/v1/keys` | every setting a config can hold: type, range, default, help |
| `GET` | `/api/v1/presets` | the presets a config can start from, with their settings |
| `GET` | `/api/v1/lastpress` | the device a button was last pressed on (empty before any press) |
| `GET` | `/api/v1/configs` | all configs |
| `POST` | `/api/v1/configs` | create one, `{"name":"<name>"}`, optionally with `"device"` to link and `"preset"` to start from |
| `GET` | `/api/v1/configs/{name}` | one config |
| `GET` | `/api/v1/configs/shared` | the shared section every unlinked device reads; read-only here (403 on a write) |
| `POST` | `/api/v1/configs/{name}/settings` | set keys from a flat object; values are strings, and `""` leaves a key to the device's default |
| `POST` | `/api/v1/configs/{name}/rename` | `{"name":"<new>"}` |
| `POST` | `/api/v1/configs/{name}/copy` | copy to `{"name":"<new>"}`, linked to nothing |
| `POST` | `/api/v1/configs/{name}/archive` | move the file to `configs/archive/`; linked devices fall back to the shared section |
| `POST` | `/api/v1/configs/{name}/autolink` | claim a serial, `{"serial":"..."}`, so its device takes this config at bridge time (409 if another config claims it) |
| `POST` | `/api/v1/configs/{name}/unautolink` | drop a serial's claim |

### The settings window

What the page reports about itself, and what it asks of the window that holds
it. The pad goes to the page only while its window is in front ("config
mode").

| Method | Path | What |
|---|---|---|
| `GET` | `/api/v1/ui/view` | the layout and controller the page last showed, for a new window to return to |
| `POST` | `/api/v1/ui/view` | the page's layout, sent on every switch |
| `POST` | `/api/v1/ui/focus` | `{"focused":true}` or `false`: config mode follows it |
| `POST` | `/api/v1/ui/field` | `{"editing":true}` while the page's cursor is in a text field |
| `POST` | `/api/v1/ui/hold` | `{"hold":true}` stops config mode and keeps it off until released |
| `POST` | `/api/v1/ui/notice` | collect a waiting notice for the page; reading it clears it |
| `POST` | `/api/v1/ui/spawn` | open a settings window, closing nothing |
| `POST` | `/api/v1/ui/reset`, `/api/v1/ui/open` | close any settings window and open a fresh one, as the pad shortcut does; used by tests |
| `POST` | `/api/v1/ui/drag` | move the window with the mouse until the button comes up |
| `POST` | `/api/v1/ui/position` | step the window to its next place |
| `POST` | `/api/v1/ui/resize` | step the window to its next size |
| `POST` | `/api/v1/ui/close` | close the window |
| `POST` | `/api/v1/ui/closed` | the page reporting its own teardown |

### GET /api/v1/status

```json
{"version":"0.0.2 (build 2)","product":"DS5-USBIP","product_version":"0.1.0",
 "transport":"tcp","control_port":48054,"usbip_port":3240,
 "uptime_seconds":812,"session_count":1}
```

`transport` is `"enet"` when the agent runs with `--enet`, else `"tcp"`.

### GET /api/v1/sessions

```json
[{"busid":"ctm-ds5-1","kind":"ds5","port":48100,"ready":true,"last_error":""}]
```

`ready` turns true once the session worker has loaded the map and profile,
started the transport and exported the device; `last_error` carries the most
recent worker failure (empty when none).

### POST /api/v1/sessions

Body (flat JSON object):

```json
{"kind":"ds5","port":48100,"busid":"ctm-ds5-1"}
```

- `kind`: one of `ds4`, `ds4_usb`, `ds5`, `ds5_usb`, `ds5e`, `ds5e_usb`,
  `hid`, `puck`, `xbox` (the same list `BRIDGE_START` accepts on the text
  channel; `tests/rest_parser_test.cpp` pins the REST side)
- `port`: 1024 to 65535, the per-controller data port the TV client connects to
- `busid`: 1 to 31 printable ASCII characters

Replies `202 Accepted` with `{"status":"starting", ...}`: session bring-up is
asynchronous, exactly like `BRIDGE_START`'s `OK bridge starting`. Poll
`GET /api/v1/sessions/{busid}` for `ready` and `last_error`. Idempotent:
posting an existing `busid` leaves the running session untouched and still
answers 202.

```bash
curl -s -X POST localhost:48055/api/v1/sessions \
     -H 'Content-Type: application/json' \
     -d '{"kind":"ds5","port":48100,"busid":"ctm-ds5-1"}'
```

### DELETE /api/v1/sessions/{busid}

`200 {"busid":"...","stopped":true}` after a full teardown (the device leaves
the USB/IP server, so Windows sees an unplug; the backend stops; the worker is
joined), or `404` if there is no such session.

### POST /api/v1/restart

Optional body `{"mode":"soft"}` (the default) or `{"mode":"hard"}`.

- **soft** tears down every bridge session and its USB/IP export; the agent
  keeps listening and rebuilds on the next start. `200`.
- **hard** restarts the whole process through the SCM. `202` in service mode,
  `409` when running interactively (the same rule as `RESTART hard` on the
  text channel).

## Security

- **Bind address.** Loopback (`127.0.0.1`) by default. `--rest-lan` binds
  `0.0.0.0`; the installer's program-based firewall rule (private profile)
  already covers any port the exe listens on, so no extra rule is needed.
- **Who may ask.** Checked on every request, before the token:
  - a request whose `Origin` is not this listener itself is refused (403),
    `null` included. A browser sends an `Origin` with every request from
    another site's page, a plain form post included, so this is what stops
    another site acting through this port;
  - a request whose `Host` is a name other than `localhost` or this PC's own
    name is refused (403). An address is always answered. This stops a page
    that a DNS server has pointed at `127.0.0.1` ("DNS rebinding"), whose
    requests carry its own name.

  curl and scripts send no `Origin`, and an address or `localhost` as `Host`,
  so they are unaffected.
- **No CORS headers.** The settings page is served from this port, so it is
  same-origin and needs none, and without them no other site's page can read
  an answer.
- **Auth.** `--rest-token <t>` requires `Authorization: Bearer <t>` on every
  `/api/` route (401 otherwise). Strongly recommended with `--rest-lan`. The
  page and its icon load without it, since a browser opening an address cannot
  send one; type the token into the page's token box and its requests carry
  it. Token comparison is length-independent. In service mode the token is
  part of the service image path and visible via `sc qc` to anyone who can
  query the SCM, the same trust level as every other service argument. The
  plaintext control channel on the agent port stays unauthenticated by design
  (the TV needs it); the token protects the REST surface only.
- **Listener hardening.** `SO_EXCLUSIVEADDRUSE` (no local port hijack of the
  token-bearing listener), 2 s receive and send timeouts, 16 KiB head and
  8 KiB body caps, chunked transfer encoding rejected.

## Threading model (read before extending)

Requests are handled **inline on the agent loop thread**, exactly like the
plaintext channel's `handle_agent_client`. That thread is the only one allowed
to start and stop sessions (see the reap-queue comment at the top of
`src/app/agent.inl`), which is what lets the handlers call
`start_bridge_session` and `stop_bridge_session` directly. If the REST listener
is ever moved to its own thread, session lifecycle changes must be queued back
to the loop (like `request_bridge_session_reap`) instead of called directly.

The trade-off is the same one the text channel already makes: a request is
served to completion before the loop returns to `select()`. The per-client
timeouts bound the worst case at a couple of seconds; the expected case
(loopback curl) is microseconds.

## Tests

The HTTP/JSON parsing half of `src/app/rest.inl`, and the checks on who may
ask, are covered by `tests/rest_parser_test.cpp`, a suite in the shared test
harness. Build and run all suites with:

```powershell
.\build-tests.ps1
```

The `REST_PARSER_ONLY` guard keeps winsock and agent state out of that
suite, so unlike the device-config tests it has no Windows dependency of its
own.
