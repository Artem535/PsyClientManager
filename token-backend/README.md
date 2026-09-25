# PsyClientManager token backend

Mints short-lived, room-scoped LiveKit JWTs for the self-hosted LiveKit
deployment. Never holds anything the desktop app needs to trust beyond a
signed token — the LiveKit API secret and the bearer credential live only
here. See `docs/asciidoc/11-token-backend-account-model-adr.adoc` and
`docs/asciidoc/12-invitation-security-model-adr.adoc` for the design this
implements.

## Build

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build
ctest --test-dir build --output-on-failure
```

## Configuration (environment variables)

| Variable | Required | Default | Purpose |
|---|---|---|---|
| `PORT` | no | `8080` | HTTP listen port |
| `DB_PATH` | no | `token-backend.sqlite3` | SQLite database file |
| `LIVEKIT_API_KEY` | yes | — | Must match the self-hosted LiveKit server's key |
| `LIVEKIT_API_SECRET` | yes | — | Must match the self-hosted LiveKit server's secret |
| `LIVEKIT_WS_ENDPOINT` | no | `ws://46.173.25.218:7880` | Returned to clients as the connection URL |
| `INVITATION_BASE_URL` | no | `https://example.invalid/join/` | Prefix for invitation links; set for real once a domain exists |
| `TOKEN_TTL_SECONDS` | no | `600` | LiveKit JWT lifetime |

## Invitation lifetime: the meeting's scheduled window

An invitation is **not** valid indefinitely. Per
`docs/asciidoc/12-invitation-security-model-adr.adoc`, the meeting's scheduled
window is the invitation's lifetime boundary: tokens are issued only while

```
scheduledStart - 5 minutes  ≤  now  ≤  scheduledEnd + 15 minutes
```

The 5-minute pre-join buffer lets a client connect slightly early; the
15-minute grace period covers sessions that run over and clients who need to
reconnect right after the scheduled end. Outside that range both
`specialist-token` and `client-token` return `410 Gone`, even though the
meeting's status is still `active`.

The invitation code stays reusable *within* that window — it is not consumed
by the first redemption, so a client who drops can rejoin. Requests outside
the window are rejected before the passcode is checked, so they do not count
against the 5-attempt limit.

`scheduledStart`/`scheduledEnd` are stored exactly as supplied at creation and
parsed as UTC in the `YYYY-MM-DDTHH:MM:SSZ` form. A value that cannot be
parsed fails closed (the window is treated as shut).

## First deploy: seed the one account

```bash
pcm_token_backend --seed-account
```

Copy the printed bearer credential into PsyClientManager's Settings once —
it is never shown again. Losing it means re-seeding, which invalidates the
previous credential (see `AccountsRepository::seedAccount`).
