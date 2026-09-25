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

## First deploy: seed the one account

```bash
pcm_token_backend --seed-account
```

Copy the printed bearer credential into PsyClientManager's Settings once —
it is never shown again. Losing it means re-seeding, which invalidates the
previous credential (see `AccountsRepository::seedAccount`).
