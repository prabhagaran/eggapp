# ClimateCloud API Reference

## Base URL
- Development: `http://localhost:3001`
- Production: `https://api.climatecloud.io`

---

## 🔐 Authentication

### Register
```
POST /auth/register
Content-Type: application/json

{
  "email": "user@example.com",
  "password": "securepassword",
  "firstName": "John",
  "lastName": "Doe",
  "orgName": "My Farm Co"
}

Response 201:
{
  "user": { "id", "email", "firstName", "lastName", "role", "orgId", "orgName" },
  "accessToken": "eyJ...",
  "refreshToken": "eyJ..."
}
```

### Login
```
POST /auth/login
{ "email": "...", "password": "..." }

Response 200: { "user": {...}, "accessToken", "refreshToken" }
```

### Refresh Token
```
POST /auth/refresh
{ "refreshToken": "eyJ..." }

Response 200: { "accessToken", "refreshToken" }
```

### Logout
```
POST /auth/logout
Authorization: Bearer <accessToken>

Response 200: { "message": "Logged out successfully" }
```

### Get Current User
```
GET /auth/me
Authorization: Bearer <accessToken>

Response 200: { "id", "email", "firstName", "role", "orgId", "orgName", "orgPlan" }
```

---

## 📡 Devices

### Register Device *(Admin+)*
```
POST /api/devices
Authorization: Bearer <accessToken>

{ "name": "Incubator #1", "type": "egg_incubator", "deviceUid": "INCUBATOR_01" }

Response 201:
{
  "device": { "id", "deviceUid", "name", "type" },
  "credentials": {
    "apiKey": "cc_abc...",          ⚠️ Shown once only
    "mqttUsername": "incubator_01",
    "mqttPassword": "xyz...",       ⚠️ Shown once only
    "mqttBrokerUrl": "mqtts://..."
  }
}
```

### List Devices
```
GET /api/devices
Authorization: Bearer <accessToken>

Response 200: { "devices": [...] }
```

### Get Device Detail
```
GET /api/devices/:deviceId
Response 200: { "device": {...}, "shadow": {...} }
```

### Update Device *(Admin+)*
```
PATCH /api/devices/:deviceId
{ "name": "New Name", "assignedUserId": "uuid" }
```

### Delete Device *(Admin+)*
```
DELETE /api/devices/:deviceId
```

### Regenerate API Key *(Admin+)*
```
POST /api/devices/:deviceId/regenerate-key
Response 200: { "credentials": {...} }
```

---

## 📊 Telemetry

### Get Latest Reading
```
GET /api/devices/:deviceId/telemetry/latest
Response 200: { "telemetry": { "temperature", "humidity", "heater_on", ... } }
```

### Get Historical Data
```
GET /api/devices/:deviceId/telemetry?from=2026-01-01T00:00:00Z&to=2026-02-24T00:00:00Z&limit=100&offset=0
Response 200: { "telemetry": [...], "pagination": { "total", "limit", "offset", "hasMore" } }
```

### Get Aggregated Data (Charts)
```
GET /api/devices/:deviceId/telemetry/aggregate?interval=24h
Intervals: 1h, 6h, 24h, 7d, 30d
Response 200: { "data": [{ "time_bucket", "avg_temperature", "min_temperature", ... }] }
```

### Dashboard Summary
```
GET /api/dashboard/summary
Response 200: { "devices": [...with latest telemetry...], "stats": { "total", "online", "offline" } }
```

---

## 🎮 Commands

### Send Command *(Technician+)*
```
POST /api/devices/:deviceId/commands
{
  "commandType": "set_setpoint",      // set_setpoint | set_mode | set_humidity_setpoint | restart | custom
  "payload": { "setpoint": 38.0 }
}

Response 201: { "command": { "id", "commandType", "status", "createdAt" } }
```

### Get Command History
```
GET /api/devices/:deviceId/commands?limit=50
Response 200: { "commands": [...] }
```

### Get Command Status
```
GET /api/commands/:commandId
Response 200: { "command": { "id", "status", "ackAt", ... } }
```

---

## 🌓 Device Shadow

### Get Shadow State
```
GET /api/devices/:deviceId/shadow
Response 200:
{
  "shadow": {
    "desired": { "setpoint": 38.0, "mode": "AUTO" },
    "reported": { "setpoint": 37.5, "temperature": 37.8 },
    "delta": { "setpoint": 38.0 },
    "version": 42
  }
}
```

### Update Desired State *(Technician+)*
```
PUT /api/devices/:deviceId/shadow/desired
{ "desired": { "setpoint": 38.0 } }
```

### Force Shadow Sync *(Admin+)*
```
POST /api/devices/:deviceId/shadow/sync
```

---

## 👥 Admin

### List Organization Users *(Admin+)*
```
GET /api/admin/users
```

### Add User *(Admin+)*
```
POST /api/admin/users
{ "email", "firstName", "lastName", "role": "technician", "password" }
```

### Update User *(Admin+)*
```
PATCH /api/admin/users/:userId
{ "role": "admin", "isActive": true }
```

### Delete User *(Owner only)*
```
DELETE /api/admin/users/:userId
```

### Get Organization Info *(Admin+)*
```
GET /api/admin/organization
```

---

## 🔄 WebSocket (Real-time)

### Connect
```
ws://localhost:3001/ws?token=<accessToken>
```

### Message Types (Server → Client)
```json
{ "type": "telemetry", "deviceId": "...", "deviceUid": "...", "data": {...} }
{ "type": "device_status", "deviceUid": "...", "status": "online|offline" }
{ "type": "command_ack", "commandId": "...", "status": "applied|failed" }
{ "type": "shadow_update", "deviceUid": "...", "shadow": {...} }
```

---

## 🏥 Health Check
```
GET /health
Response: { "status": "ok", "mqtt": "connected", "database": "connected" }
```

---

## Role Permissions

| Endpoint | Owner | Admin | Technician | Viewer |
|----------|-------|-------|------------|--------|
| View devices | ✅ | ✅ | ✅ (assigned) | ✅ (assigned) |
| Register device | ✅ | ✅ | ❌ | ❌ |
| Send commands | ✅ | ✅ | ✅ | ❌ |
| Update shadow | ✅ | ✅ | ✅ | ❌ |
| Manage users | ✅ | ✅ | ❌ | ❌ |
| Delete users | ✅ | ❌ | ❌ | ❌ |
| View telemetry | ✅ | ✅ | ✅ | ✅ |
