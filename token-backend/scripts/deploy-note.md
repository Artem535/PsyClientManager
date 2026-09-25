# Deploying alongside the existing LiveKit containers

This follows the same `/opt/livekit/docker-compose.yml` pattern already
running on the PSY server for `livekit` and `meet`. Add a sibling service —
do not put this inside the `livekit` container.

```yaml
  token-backend:
    build:
      context: /opt/pcm-token-backend
      dockerfile: Dockerfile
    container_name: pcm-token-backend
    network_mode: host
    restart: unless-stopped
    environment:
      LIVEKIT_API_KEY: <same value as the livekit service>
      LIVEKIT_API_SECRET: <same value as the livekit service>
      LIVEKIT_WS_ENDPOINT: ws://46.173.25.218:7880
      DB_PATH: /data/token-backend.sqlite3
    volumes:
      - /opt/pcm-token-backend/data:/data
```

Copy `token-backend/` from this repo to `/opt/pcm-token-backend` on the PSY
server (same layout used for the LiveKit Meet build earlier), then
`podman-compose build token-backend && podman-compose up -d token-backend`.
Open `8080/tcp` in firewalld the same way `3000/tcp` was opened for Meet.

TLS is intentionally not part of this: it stays behind Caddy once a domain
exists, exactly like the deferred `wss://` work for LiveKit itself.
