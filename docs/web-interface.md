# Web Interface

Optional module (`LINGUA_BUILD_WEB=ON`). Binds to `127.0.0.1` (localhost) only by default. Remote binding requires explicit opt-in and a token.

## REST API

- `POST /api/detect` - returns detected format.
- `POST /api/inspect` - returns inspection report JSON.
- `POST /api/import` - stages and imports; `dry_run=1` query for dry run.

## Security

- Never executes uploaded files.
- Size limits enforced.
- Safe path handling against traversal.
- Staging directory with restricted permissions.
- Audit log of every import.
- Imported apps are untrusted code.