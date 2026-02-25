// ─────────────────────────────────────────────────────────
// ClimateCloud – Database Schema (Drizzle ORM + PostgreSQL)
// ─────────────────────────────────────────────────────────

import {
    pgTable,
    uuid,
    varchar,
    text,
    boolean,
    integer,
    real,
    timestamp,
    jsonb,
    index,
    uniqueIndex,
    pgEnum,
} from 'drizzle-orm/pg-core';
import { relations } from 'drizzle-orm';

// ──────── Enums ────────

export const planEnum = pgEnum('plan_type', ['free', 'starter', 'professional', 'enterprise']);
export const roleEnum = pgEnum('role_type', ['owner', 'admin', 'technician', 'viewer']);
export const commandStatusEnum = pgEnum('command_status', ['pending', 'sent', 'delivered', 'applied', 'failed', 'timeout']);
export const deviceTypeEnum = pgEnum('device_type', ['egg_incubator', 'climate_chamber', 'thermostat', 'generic']);

// ──────── Organizations ────────

export const organizations = pgTable('organizations', {
    id: uuid('id').defaultRandom().primaryKey(),
    name: varchar('name', { length: 255 }).notNull(),
    slug: varchar('slug', { length: 100 }).notNull(),
    plan: planEnum('plan').default('free').notNull(),
    maxDevices: integer('max_devices').default(5).notNull(),
    maxUsers: integer('max_users').default(3).notNull(),
    metadata: jsonb('metadata').default({}),
    createdAt: timestamp('created_at', { withTimezone: true }).defaultNow().notNull(),
    updatedAt: timestamp('updated_at', { withTimezone: true }).defaultNow().notNull(),
}, (table) => [
    uniqueIndex('org_slug_idx').on(table.slug),
]);

// ──────── Users ────────

export const users = pgTable('users', {
    id: uuid('id').defaultRandom().primaryKey(),
    email: varchar('email', { length: 320 }).notNull(),
    passwordHash: text('password_hash').notNull(),
    firstName: varchar('first_name', { length: 100 }),
    lastName: varchar('last_name', { length: 100 }),
    orgId: uuid('org_id').references(() => organizations.id, { onDelete: 'cascade' }).notNull(),
    role: roleEnum('role').default('viewer').notNull(),
    isActive: boolean('is_active').default(true).notNull(),
    lastLogin: timestamp('last_login', { withTimezone: true }),
    refreshToken: text('refresh_token'),
    createdAt: timestamp('created_at', { withTimezone: true }).defaultNow().notNull(),
    updatedAt: timestamp('updated_at', { withTimezone: true }).defaultNow().notNull(),
}, (table) => [
    uniqueIndex('user_email_idx').on(table.email),
    index('user_org_idx').on(table.orgId),
]);

// ──────── Devices ────────

export const devices = pgTable('devices', {
    id: uuid('id').defaultRandom().primaryKey(),
    deviceUid: varchar('device_uid', { length: 64 }).notNull(),
    name: varchar('name', { length: 255 }).notNull(),
    type: deviceTypeEnum('type').default('generic').notNull(),
    orgId: uuid('org_id').references(() => organizations.id, { onDelete: 'cascade' }).notNull(),
    assignedUserId: uuid('assigned_user_id').references(() => users.id, { onDelete: 'set null' }),
    apiKeyHash: text('api_key_hash').notNull(),
    mqttUsername: varchar('mqtt_username', { length: 128 }).notNull(),
    mqttPasswordHash: text('mqtt_password_hash').notNull(),
    fwVersion: varchar('fw_version', { length: 32 }),
    isOnline: boolean('is_online').default(false).notNull(),
    lastSeen: timestamp('last_seen', { withTimezone: true }),
    metadata: jsonb('metadata').default({}),
    createdAt: timestamp('created_at', { withTimezone: true }).defaultNow().notNull(),
    updatedAt: timestamp('updated_at', { withTimezone: true }).defaultNow().notNull(),
}, (table) => [
    uniqueIndex('device_uid_idx').on(table.deviceUid),
    index('device_org_idx').on(table.orgId),
    index('device_assigned_user_idx').on(table.assignedUserId),
    index('device_online_idx').on(table.isOnline),
]);

// ──────── Telemetry ────────

export const telemetry = pgTable('telemetry', {
    id: uuid('id').defaultRandom().primaryKey(),
    deviceId: uuid('device_id').references(() => devices.id, { onDelete: 'cascade' }).notNull(),
    temperature: real('temperature'),
    humidity: real('humidity'),
    heaterOn: boolean('heater_on'),
    coolerOn: boolean('cooler_on'),
    humidifierOn: boolean('humidifier_on'),
    setpoint: real('setpoint'),
    humidSetpoint: real('humid_setpoint'),
    mode: varchar('mode', { length: 32 }),
    fwVersion: varchar('fw_version', { length: 32 }),
    payload: jsonb('payload').default({}),
    recordedAt: timestamp('recorded_at', { withTimezone: true }).defaultNow().notNull(),
}, (table) => [
    index('telemetry_device_time_idx').on(table.deviceId, table.recordedAt),
    index('telemetry_recorded_idx').on(table.recordedAt),
]);

// ──────── Device Shadow ────────

export const deviceShadows = pgTable('device_shadows', {
    id: uuid('id').defaultRandom().primaryKey(),
    deviceId: uuid('device_id').references(() => devices.id, { onDelete: 'cascade' }).notNull(),
    desired: jsonb('desired').default({}).notNull(),
    reported: jsonb('reported').default({}).notNull(),
    delta: jsonb('delta').default({}),
    version: integer('version').default(1).notNull(),
    lastSync: timestamp('last_sync', { withTimezone: true }),
    updatedAt: timestamp('updated_at', { withTimezone: true }).defaultNow().notNull(),
}, (table) => [
    uniqueIndex('shadow_device_idx').on(table.deviceId),
]);

// ──────── Commands ────────

export const commands = pgTable('commands', {
    id: uuid('id').defaultRandom().primaryKey(),
    deviceId: uuid('device_id').references(() => devices.id, { onDelete: 'cascade' }).notNull(),
    userId: uuid('user_id').references(() => users.id, { onDelete: 'set null' }),
    commandType: varchar('command_type', { length: 64 }).notNull(),
    payload: jsonb('payload').default({}).notNull(),
    status: commandStatusEnum('status').default('pending').notNull(),
    response: jsonb('response'),
    sentAt: timestamp('sent_at', { withTimezone: true }),
    ackAt: timestamp('ack_at', { withTimezone: true }),
    createdAt: timestamp('created_at', { withTimezone: true }).defaultNow().notNull(),
}, (table) => [
    index('cmd_device_idx').on(table.deviceId),
    index('cmd_status_idx').on(table.status),
    index('cmd_created_idx').on(table.createdAt),
]);

// ──────── Audit Logs ────────

export const auditLogs = pgTable('audit_logs', {
    id: uuid('id').defaultRandom().primaryKey(),
    orgId: uuid('org_id').references(() => organizations.id, { onDelete: 'cascade' }).notNull(),
    userId: uuid('user_id').references(() => users.id, { onDelete: 'set null' }),
    action: varchar('action', { length: 100 }).notNull(),
    resourceType: varchar('resource_type', { length: 64 }).notNull(),
    resourceId: varchar('resource_id', { length: 128 }),
    details: jsonb('details').default({}),
    ipAddress: varchar('ip_address', { length: 45 }),
    createdAt: timestamp('created_at', { withTimezone: true }).defaultNow().notNull(),
}, (table) => [
    index('audit_org_idx').on(table.orgId),
    index('audit_user_idx').on(table.userId),
    index('audit_created_idx').on(table.createdAt),
]);

// ──────── Alert Rules (Phase 2) ────────

export const alertRules = pgTable('alert_rules', {
    id: uuid('id').defaultRandom().primaryKey(),
    orgId: uuid('org_id').references(() => organizations.id, { onDelete: 'cascade' }).notNull(),
    deviceId: uuid('device_id').references(() => devices.id, { onDelete: 'cascade' }),
    name: varchar('name', { length: 255 }).notNull(),
    condition: jsonb('condition').notNull(), // { field, operator, value }
    isActive: boolean('is_active').default(true).notNull(),
    notifyEmail: boolean('notify_email').default(true),
    lastTriggered: timestamp('last_triggered', { withTimezone: true }),
    createdAt: timestamp('created_at', { withTimezone: true }).defaultNow().notNull(),
});

// ──────── Relations ────────

export const organizationRelations = relations(organizations, ({ many }) => ({
    users: many(users),
    devices: many(devices),
    auditLogs: many(auditLogs),
    alertRules: many(alertRules),
}));

export const userRelations = relations(users, ({ one, many }) => ({
    organization: one(organizations, {
        fields: [users.orgId],
        references: [organizations.id],
    }),
    assignedDevices: many(devices),
    commands: many(commands),
    auditLogs: many(auditLogs),
}));

export const deviceRelations = relations(devices, ({ one, many }) => ({
    organization: one(organizations, {
        fields: [devices.orgId],
        references: [organizations.id],
    }),
    assignedUser: one(users, {
        fields: [devices.assignedUserId],
        references: [users.id],
    }),
    shadow: one(deviceShadows, {
        fields: [devices.id],
        references: [deviceShadows.deviceId],
    }),
    telemetry: many(telemetry),
    commands: many(commands),
}));

export const telemetryRelations = relations(telemetry, ({ one }) => ({
    device: one(devices, {
        fields: [telemetry.deviceId],
        references: [devices.id],
    }),
}));

export const deviceShadowRelations = relations(deviceShadows, ({ one }) => ({
    device: one(devices, {
        fields: [deviceShadows.deviceId],
        references: [devices.id],
    }),
}));

export const commandRelations = relations(commands, ({ one }) => ({
    device: one(devices, {
        fields: [commands.deviceId],
        references: [devices.id],
    }),
    user: one(users, {
        fields: [commands.userId],
        references: [users.id],
    }),
}));

export const auditLogRelations = relations(auditLogs, ({ one }) => ({
    organization: one(organizations, {
        fields: [auditLogs.orgId],
        references: [organizations.id],
    }),
    user: one(users, {
        fields: [auditLogs.userId],
        references: [users.id],
    }),
}));
