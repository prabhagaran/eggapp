#!/usr/bin/env bash
# Redeploy apps/api to the Radxa (ADR 0006). Run from the repo root:
#   bash infra/deploy/deploy-api.sh
#
# What it does: tars the source apps/api actually needs (itself + the two
# workspace packages it depends on), ships it to the Radxa, rebuilds
# in-place, and restarts the systemd service. Does NOT touch
# apps/api/.env on the Radxa (production secrets) or run migrations —
# run `pnpm --filter @eggapp/db db:deploy` yourself first if this
# deploy includes a schema change.
set -euo pipefail

HOST="radxa@192.168.1.44"
REMOTE_DIR="~/eggapp-app"
TARBALL="/tmp/eggapp-api-deploy.tar.gz"

echo "==> Packing source"
tar --exclude='node_modules' --exclude='dist' --exclude='.env' --exclude='*.log' \
  -czf "$TARBALL" \
  package.json pnpm-workspace.yaml pnpm-lock.yaml tsconfig.base.json \
  packages/db packages/shared-types apps/api

echo "==> Shipping to Radxa"
scp "$TARBALL" "$HOST:$REMOTE_DIR/deploy.tar.gz"
rm "$TARBALL"

echo "==> Extracting + installing + building on Radxa"
ssh "$HOST" "cd $REMOTE_DIR && tar -xzf deploy.tar.gz && rm deploy.tar.gz && \
  pnpm install --frozen-lockfile && \
  pnpm --filter @eggapp/db build && \
  pnpm --filter @eggapp/shared-types build && \
  pnpm --filter @eggapp/api build"

echo "==> Restarting service (will prompt for the Radxa sudo password)"
ssh -t "$HOST" "sudo systemctl restart eggapp-api && sleep 2 && sudo systemctl status eggapp-api --no-pager | head -6"

echo "==> Done. Tail logs with: ssh $HOST 'sudo journalctl -u eggapp-api -f'"
