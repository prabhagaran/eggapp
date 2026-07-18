#!/usr/bin/env bash
# Redeploy apps/web to the Radxa, same pattern as deploy-api.sh. Run from
# the repo root:
#   bash infra/deploy/deploy-web.sh
#
# What it does: tars the source apps/web actually needs (itself + the
# one workspace package it depends on), ships it to the Radxa, rebuilds
# in-place (next build bakes NEXT_PUBLIC_API_URL in at this step), and
# restarts the systemd service. Does NOT touch apps/web/.env.production
# on the Radxa — that's created once directly on the device (see
# infra/deploy/README.md), same reasoning as apps/api/.env: a value
# copied from a dev machine's .env is exactly the kind of drift that
# bites later.
set -euo pipefail

HOST="radxa@192.168.1.44"
REMOTE_DIR="~/eggapp-app"
TARBALL="/tmp/eggapp-web-deploy.tar.gz"

echo "==> Packing source"
tar --exclude='node_modules' --exclude='.next' --exclude='.env*' --exclude='*.log' \
  -czf "$TARBALL" \
  package.json pnpm-workspace.yaml pnpm-lock.yaml tsconfig.base.json \
  packages/shared-types apps/web

echo "==> Shipping to Radxa"
scp "$TARBALL" "$HOST:$REMOTE_DIR/deploy.tar.gz"
rm "$TARBALL"

echo "==> Extracting + installing + building on Radxa"
ssh "$HOST" "cd $REMOTE_DIR && tar -xzf deploy.tar.gz && rm deploy.tar.gz && \
  pnpm install --frozen-lockfile && \
  pnpm --filter @eggapp/shared-types build && \
  pnpm --filter @eggapp/web build"

echo "==> Restarting service"
ssh "$HOST" "sudo systemctl restart eggapp-web && sleep 2 && sudo systemctl status eggapp-web --no-pager | head -6"

echo "==> Done. Tail logs with: ssh $HOST 'sudo journalctl -u eggapp-web -f'"
