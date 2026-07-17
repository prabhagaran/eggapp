// FCM push dispatch (US-NOT-002). Opt-in via FCM_SERVICE_ACCOUNT_PATH —
// absent path disables push cleanly, same pattern as MQTT ingest.
import { readFileSync } from "node:fs";
import { cert, getApps, initializeApp } from "firebase-admin/app";
import { getMessaging } from "firebase-admin/messaging";
import type { FastifyBaseLogger } from "fastify";
import { config } from "../../config.js";

let ready = false;

function ensureInitialized(): boolean {
  if (ready) return true;
  if (!config.fcmServiceAccountPath) return false;
  if (getApps().length === 0) {
    const serviceAccount = JSON.parse(readFileSync(config.fcmServiceAccountPath, "utf-8"));
    initializeApp({ credential: cert(serviceAccount) });
  }
  ready = true;
  return true;
}

export interface PushPayload {
  title: string;
  body: string;
  deepLink?: string; // e.g. "eggapp://alerts" — consumed by the Android app (future increment)
}

/** Sends to one device token. Never throws — a push failure must never break the caller (alert evaluation). */
export async function sendPush(log: FastifyBaseLogger, token: string, payload: PushPayload): Promise<void> {
  if (!ensureInitialized()) {
    log.info("[fcm] FCM_SERVICE_ACCOUNT_PATH not set — push disabled");
    return;
  }
  try {
    await getMessaging().send({
      token,
      notification: { title: payload.title, body: payload.body },
      data: payload.deepLink ? { deepLink: payload.deepLink } : undefined,
    });
  } catch (err) {
    log.error({ err }, "[fcm] send failed");
  }
}
