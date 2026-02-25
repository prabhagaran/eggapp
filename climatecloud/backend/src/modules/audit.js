// ─────────────────────────────────────────────
// ClimateCloud – Audit Logging
// ─────────────────────────────────────────────

import { auditLogs } from '../db/schema.js';

/**
 * Create an audit log entry
 */
export async function createAuditLog(db, {
    orgId,
    userId = null,
    action,
    resourceType,
    resourceId = null,
    details = {},
    ipAddress = null,
}) {
    try {
        if (!db) return;

        await db.insert(auditLogs).values({
            orgId,
            userId,
            action,
            resourceType,
            resourceId,
            details,
            ipAddress,
        });
    } catch (err) {
        // Non-blocking – audit log failures should not break operations
        console.error('[AUDIT] Failed to create audit log:', err.message);
    }
}
