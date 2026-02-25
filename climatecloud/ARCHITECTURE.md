# ClimateCloud — IoT SaaS Platform Architecture

## 🏗 System Overview

ClimateCloud is a production-grade, multi-tenant IoT cloud platform designed for climate control devices (egg incubators, climate chambers, thermostats). It provides real-time monitoring, remote control, device shadow synchronization, and role-based multi-user access.

---

## 📐 Architecture Diagram

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                           CLIMATECLOUD PLATFORM                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────┐    ┌──────────────────┐    ┌──────────────────────┐    │
│  │   FRONTEND       │    │   BACKEND API     │    │   MQTT BROKER        │    │
│  │   (Next.js)      │◄──►│   (Fastify)       │◄──►│   (EMQX Cloud)       │    │
│  │                   │    │                    │    │                      │    │
│  │  • Dashboard      │    │  • Auth Module     │    │  • TLS Encryption    │    │
│  │  • Device Control │    │  • Device Mgmt     │    │  • ACL per Device    │    │
│  │  • Admin Console  │    │  • Telemetry       │    │  • QoS Levels        │    │
│  │  • Real-time      │    │  • Commands        │    │  • Will Messages     │    │
│  │    Charts         │    │  • Device Shadow   │    │  • WebSocket Bridge  │    │
│  │  • Auth Pages     │    │  • Rule Engine     │    │                      │    │
│  └────────┬──────────┘    └────────┬───────────┘    └──────────┬───────────┘    │
│           │                        │                           │                │
│           │  WebSocket/REST        │  MQTT Client              │                │
│           │                        │                           │                │
│  ┌────────▼────────────────────────▼───────────────────────────▼───────────┐    │
│  │                        DATA LAYER                                       │    │
│  │                                                                         │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  │    │
│  │  │  PostgreSQL   │  │    Redis      │  │  Object      │                  │    │
│  │  │  (Supabase)   │  │  (Cache +     │  │  Storage     │                  │    │
│  │  │              │  │   Pub/Sub)    │  │  (S3/R2)     │                  │    │
│  │  │  • Users     │  │              │  │              │                  │    │
│  │  │  • Devices   │  │  • Sessions  │  │  • Firmware  │                  │    │
│  │  │  • Telemetry │  │  • Cache     │  │  • Logs      │                  │    │
│  │  │  • Shadows   │  │  • Rate Lim  │  │              │                  │    │
│  │  │  • Commands  │  │              │  │              │                  │    │
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  │    │
│  └─────────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
├──────────────────────────────────────────────────────────────────────────────┤
│                          DEVICE LAYER (ESP32)                                │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  ESP32 Firmware (FreeRTOS)                                          │    │
│  │                                                                     │    │
│  │  ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────────┐   │    │
│  │  │ Sensor    │  │ Control   │  │ MQTT      │  │ Shadow        │   │    │
│  │  │ Task      │  │ Task      │  │ Task      │  │ Sync Task     │   │    │
│  │  └───────────┘  └───────────┘  └───────────┘  └───────────────┘   │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 📊 Data Flow Diagrams

### 1. Telemetry Data Flow

```
ESP32 Device                     EMQX Broker                  Backend API                 PostgreSQL
    │                                │                             │                           │
    │  PUBLISH telemetry JSON        │                             │                           │
    │  topic: device/{id}/telemetry  │                             │                           │
    │  QoS: 1                        │                             │                           │
    ├───────────────────────────────►│                             │                           │
    │                                │  Forward to subscriber      │                           │
    │                                ├────────────────────────────►│                           │
    │                                │                             │  INSERT telemetry row     │
    │                                │                             ├──────────────────────────►│
    │                                │                             │                           │
    │                                │                             │  Update device shadow     │
    │                                │                             │  (reported state)         │
    │                                │                             ├──────────────────────────►│
    │                                │                             │                           │
    │                                │                             │  Push via WebSocket       │
    │                                │                             │  to connected dashboards  │
    │                                │                             ├─────────►[Frontend]       │
```

### 2. Command Flow

```
Frontend Dashboard               Backend API                 EMQX Broker              ESP32 Device
    │                                │                           │                         │
    │  POST /api/devices/{id}/cmd    │                           │                         │
    │  { setpoint: 38.0, mode: AUTO }│                           │                         │
    ├───────────────────────────────►│                           │                         │
    │                                │  Update shadow desired    │                         │
    │                                │  Store command record     │                         │
    │                                │                           │                         │
    │                                │  PUBLISH command          │                         │
    │                                │  topic: device/{id}/cmd   │                         │
    │                                │  QoS: 1                   │                         │
    │                                ├──────────────────────────►│                         │
    │                                │                           │  Deliver to device      │
    │                                │                           ├────────────────────────►│
    │                                │                           │                         │
    │                                │                           │  Device acks command    │
    │                                │                           │  PUBLISH to             │
    │                                │                           │  device/{id}/cmd/ack    │
    │                                │                           │◄────────────────────────┤
    │                                │◄──────────────────────────┤                         │
    │                                │  Update command status    │                         │
    │◄───────────────────────────────┤  Notify via WebSocket     │                         │
```

### 3. Device Shadow Sync Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                    DEVICE SHADOW STATE                            │
│                                                                  │
│  {                                                               │
│    "desired": {                                                  │
│      "setpoint": 38.0,        ← Set by cloud/user              │
│      "mode": "AUTO",          ← Set by cloud/user              │
│      "humid_setpoint": 65.0   ← Set by cloud/user              │
│    },                                                            │
│    "reported": {                                                 │
│      "setpoint": 37.5,        ← Reported by device             │
│      "mode": "AUTO",          ← Reported by device             │
│      "temperature": 37.8,     ← Reported by device             │
│      "humidity": 62,          ← Reported by device             │
│      "heater": true,          ← Reported by device             │
│      "cooler": false,         ← Reported by device             │
│      "fw_version": "1.0.0"    ← Reported by device             │
│    },                                                            │
│    "delta": {                                                    │
│      "setpoint": 38.0         ← Computed difference            │
│    },                                                            │
│    "metadata": {                                                 │
│      "last_sync": "2026-02-24T14:12:00Z",                      │
│      "version": 42                                               │
│    }                                                             │
│  }                                                               │
└──────────────────────────────────────────────────────────────────┘

Shadow Sync Flow:

1. User changes setpoint → Backend updates shadow.desired
2. Backend computes delta (desired vs reported)
3. If delta exists → PUBLISH delta to device/{id}/shadow/delta
4. Device receives delta → applies changes → reports new state
5. Backend receives report → updates shadow.reported
6. Backend recomputes delta → if empty, state is synced
```

### 4. Authentication Flow

```
┌──────────┐          ┌──────────────┐          ┌──────────────┐
│ Frontend │          │  Backend API │          │  PostgreSQL  │
└────┬─────┘          └──────┬───────┘          └──────┬───────┘
     │                       │                         │
     │  POST /auth/register  │                         │
     │  {email, password,    │                         │
     │   org_name}           │                         │
     ├──────────────────────►│                         │
     │                       │  Hash password          │
     │                       │  Create org + user      │
     │                       ├────────────────────────►│
     │                       │                         │
     │  {access_token,       │                         │
     │   refresh_token}      │                         │
     │◄──────────────────────┤                         │
     │                       │                         │
     │  GET /api/devices     │                         │
     │  Authorization:       │                         │
     │   Bearer <token>      │                         │
     ├──────────────────────►│                         │
     │                       │  Verify JWT             │
     │                       │  Check role permissions │
     │                       │  Query user's devices   │
     │                       ├────────────────────────►│
     │                       │                         │
     │  {devices: [...]}     │                         │
     │◄──────────────────────┤                         │
```

---

## 🔌 Component Interaction Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        COMPONENT INTERACTIONS                           │
│                                                                         │
│                    ┌──────────────┐                                     │
│                    │   Next.js    │                                     │
│                    │   Frontend   │                                     │
│                    └──────┬───────┘                                     │
│                           │                                             │
│              ┌────────────┼────────────┐                               │
│              │            │            │                               │
│         REST API     WebSocket    MQTT/WS                              │
│              │            │        (optional)                          │
│              │            │            │                               │
│              ▼            ▼            │                               │
│    ┌─────────────────────────────┐     │                               │
│    │      Fastify Backend       │     │                               │
│    │                             │     │                               │
│    │  ┌──────┐ ┌──────┐ ┌────┐ │     │                               │
│    │  │ Auth │ │Device│ │Tele│ │     │                               │
│    │  │Module│ │ Mgmt │ │metry│ │     │                               │
│    │  └──────┘ └──────┘ └────┘ │     │                               │
│    │  ┌──────┐ ┌──────┐ ┌────┐ │     │                               │
│    │  │Cmd   │ │Shadow│ │Rule│ │     │                               │
│    │  │Module│ │Module│ │ Eng│ │     │                               │
│    │  └──────┘ └──────┘ └────┘ │     │                               │
│    └────────────┬────────────────┘     │                               │
│                 │                      │                               │
│        ┌────────┼────────┐            │                               │
│        │        │        │            │                               │
│        ▼        ▼        ▼            ▼                               │
│   ┌────────┐ ┌──────┐ ┌──────────────────┐                           │
│   │Postgres│ │Redis │ │   EMQX Broker    │                           │
│   └────────┘ └──────┘ └────────┬─────────┘                           │
│                                │                                     │
│                     ┌──────────┼──────────┐                           │
│                     │          │          │                           │
│                     ▼          ▼          ▼                           │
│              ┌──────────┐ ┌──────────┐ ┌──────────┐                  │
│              │ Device 1 │ │ Device 2 │ │ Device N │                  │
│              │ (ESP32)  │ │ (ESP32)  │ │ (ESP32)  │                  │
│              └──────────┘ └──────────┘ └──────────┘                  │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 🗄 Database Schema (PostgreSQL)

### Entity-Relationship Diagram

```
┌──────────────────┐       ┌──────────────────┐       ┌──────────────────┐
│   organizations  │       │      users       │       │      roles       │
├──────────────────┤       ├──────────────────┤       ├──────────────────┤
│ id (PK)          │◄──┐   │ id (PK)          │       │ id (PK)          │
│ name             │   │   │ email            │       │ name             │
│ slug             │   │   │ password_hash    │       │ permissions      │
│ plan             │   ├───┤ org_id (FK)      │   ┌───┤ org_id (FK)      │
│ max_devices      │   │   │ role_id (FK) ────┼───┘   │                  │
│ created_at       │   │   │ is_active        │       │                  │
│ updated_at       │   │   │ last_login       │       │                  │
└──────────────────┘   │   │ created_at       │       └──────────────────┘
                       │   └──────────────────┘
                       │
                       │   ┌──────────────────┐       ┌──────────────────┐
                       │   │     devices      │       │  device_shadows  │
                       │   ├──────────────────┤       ├──────────────────┤
                       │   │ id (PK)          │◄──────┤ device_id (FK)   │
                       ├───┤ org_id (FK)      │       │ desired (JSONB)  │
                       │   │ device_uid       │       │ reported (JSONB) │
                       │   │ name             │       │ delta (JSONB)    │
                       │   │ type             │       │ version          │
                       │   │ api_key_hash     │       │ last_sync        │
                       │   │ fw_version       │       │ updated_at       │
                       │   │ is_online        │       └──────────────────┘
                       │   │ last_seen        │
                       │   │ assigned_user_id │       ┌──────────────────┐
                       │   │ metadata (JSONB) │       │    telemetry     │
                       │   │ created_at       │       ├──────────────────┤
                       │   └────────┬─────────┘       │ id (PK)          │
                       │            │                  │ device_id (FK)───┤►
                       │            │                  │ temperature      │
                       │            │                  │ humidity         │
                       │            │                  │ heater_on        │
                       │            │                  │ cooler_on        │
                       │            │                  │ setpoint         │
                       │            │                  │ fw_version       │
                       │            │                  │ payload (JSONB)  │
                       │            │                  │ recorded_at      │
                       │            │                  └──────────────────┘
                       │            │
                       │            │                  ┌──────────────────┐
                       │            │                  │    commands      │
                       │            │                  ├──────────────────┤
                       │            └─────────────────►│ id (PK)          │
                       │                               │ device_id (FK)   │
                       │                               │ user_id (FK)     │
                       │                               │ command_type     │
                       │                               │ payload (JSONB)  │
                       │                               │ status           │
                       │                               │ ack_at           │
                       │                               │ created_at       │
                       │                               └──────────────────┘
                       │
                       │                               ┌──────────────────┐
                       │                               │   audit_logs     │
                       │                               ├──────────────────┤
                       └──────────────────────────────►│ id (PK)          │
                                                       │ org_id (FK)      │
                                                       │ user_id (FK)     │
                                                       │ action           │
                                                       │ resource_type    │
                                                       │ resource_id      │
                                                       │ details (JSONB)  │
                                                       │ ip_address       │
                                                       │ created_at       │
                                                       └──────────────────┘
```

---

## 📡 MQTT Topic Hierarchy

```
climatecloud/
├── device/{device_uid}/
│   ├── telemetry          # Device → Cloud  (QoS 1)
│   │                      # Payload: { temperature, humidity, heater, cooler, ... }
│   │
│   ├── status             # Device → Cloud  (QoS 1, Retained)
│   │                      # Payload: { status: "online" | "offline" }
│   │                      # Last Will: { status: "offline" }
│   │
│   ├── command            # Cloud → Device  (QoS 1)
│   │                      # Payload: { command_id, setpoint, mode, ... }
│   │
│   ├── command/ack        # Device → Cloud  (QoS 1)
│   │                      # Payload: { command_id, status: "applied" | "error" }
│   │
│   ├── shadow/
│   │   ├── get            # Device → Cloud  (QoS 1)
│   │   │                  # Device requests current shadow
│   │   │
│   │   ├── update         # Device → Cloud  (QoS 1)
│   │   │                  # Device reports its state
│   │   │
│   │   └── delta          # Cloud → Device  (QoS 1)
│   │                      # Cloud sends desired-reported diff
│   │
│   └── ota/               # Phase 3
│       ├── notify         # Cloud → Device  (new firmware available)
│       └── status         # Device → Cloud  (download progress)
```

### MQTT Design Decisions

| Aspect | Decision | Rationale |
|--------|----------|-----------|
| **QoS for Telemetry** | QoS 1 (At least once) | Ensures data delivery, acceptable duplicate handling |
| **QoS for Commands** | QoS 1 | Commands must be delivered; idempotent design handles duplicates |
| **QoS for Status** | QoS 1 + Retained | Last status always available for new subscribers |
| **Last Will** | `device/{id}/status` → `{ "status": "offline" }` | Automatic offline detection |
| **Authentication** | Username/password per device | Each device has unique MQTT credentials derived from API key |
| **Topic ACL** | Device can only publish/subscribe to its own topics | Prevents cross-device access |

---

## 🔐 Security Architecture

### Authentication Strategy

```
┌─────────────────────────────────────────────────────────────────┐
│                    SECURITY LAYERS                               │
│                                                                 │
│  Layer 1: Transport Security                                    │
│  ├── HTTPS/TLS for all API calls                               │
│  ├── MQTTS (MQTT over TLS) for device communication            │
│  └── WSS (WebSocket Secure) for real-time dashboard            │
│                                                                 │
│  Layer 2: Identity & Authentication                             │
│  ├── User Auth: JWT (access + refresh tokens)                  │
│  │   ├── Access Token: 15 min expiry                           │
│  │   ├── Refresh Token: 7 day expiry, rotated                  │
│  │   └── Stored in httpOnly secure cookies                     │
│  │                                                             │
│  ├── Device Auth: API Key + MQTT Credentials                   │
│  │   ├── API Key: SHA-256 hashed, stored in DB                 │
│  │   ├── MQTT Username: device_uid                             │
│  │   └── MQTT Password: derived from API key                  │
│  │                                                             │
│  └── Service Auth: Internal API keys between services          │
│                                                                 │
│  Layer 3: Authorization                                         │
│  ├── RBAC (Role-Based Access Control)                          │
│  │   ├── Admin: Full access, user management                  │
│  │   ├── Technician: Device control, view all assigned         │
│  │   └── Viewer: Read-only dashboard access                   │
│  │                                                             │
│  └── Organization Isolation (Multi-tenancy)                    │
│      └── All queries scoped by org_id                          │
│                                                                 │
│  Layer 4: Rate Limiting & DDoS Protection                      │
│  ├── API: 100 req/min per user, 1000 req/min per org          │
│  ├── Auth: 5 login attempts per 15 min                        │
│  ├── MQTT: Message rate limiting per device                    │
│  └── Cloudflare/Vercel edge protection                        │
│                                                                 │
│  Layer 5: Data Protection                                       │
│  ├── Passwords: bcrypt with salt (12 rounds)                   │
│  ├── API Keys: crypto.randomBytes(32), SHA-256 stored          │
│  ├── Telemetry: Encrypted at rest (Supabase)                   │
│  └── Audit logging for all sensitive operations                │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📈 Scalability Design

### Scaling Strategy for 10,000+ Devices

```
┌─────────────────────────────────────────────────────────────────┐
│                    SCALABILITY APPROACH                          │
│                                                                 │
│  1. MQTT Broker Scaling                                         │
│  ├── EMQX Cloud auto-scales (managed service)                  │
│  ├── Clustered deployment for self-hosted                      │
│  ├── 10K connections ≈ EMQX Standard Plan                      │
│  └── Shared subscriptions for backend consumers                │
│                                                                 │
│  2. Backend API Scaling                                         │
│  ├── Stateless Fastify instances                               │
│  ├── Horizontal scaling via container orchestration            │
│  ├── Load balancer distributes requests                        │
│  ├── Each instance subscribes to MQTT shared group             │
│  └── Redis for shared state and session management             │
│                                                                 │
│  3. Database Scaling                                            │
│  ├── Telemetry Partitioning:                                   │
│  │   ├── Time-based partitioning (monthly)                     │
│  │   ├── Auto-drop partitions older than retention period      │
│  │   └── Indexes on (device_id, recorded_at)                  │
│  │                                                             │
│  ├── Connection Pooling:                                       │
│  │   ├── PgBouncer for connection management                   │
│  │   └── Supabase built-in pooling                            │
│  │                                                             │
│  └── Read Replicas:                                            │
│      └── Dashboard reads from replica, writes to primary       │
│                                                                 │
│  4. Caching Strategy                                            │
│  ├── Redis cache for:                                          │
│  │   ├── Latest device state (TTL: 60s)                       │
│  │   ├── User sessions and permissions                        │
│  │   ├── Device shadow (in-memory for fast access)            │
│  │   └── API response caching for dashboard                   │
│  │                                                             │
│  └── Cache invalidation:                                       │
│      └── Write-through on telemetry/shadow updates             │
│                                                                 │
│  5. Message Processing                                          │
│  ├── Batch telemetry inserts (every 5s)                        │
│  ├── Message queue for async operations                        │
│  └── WebSocket connection pooling                              │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🚀 Development Phase Plan

### Phase 1 — MVP (Weeks 1-4)
- Single organization, multiple devices
- User registration & login (JWT)
- Device registration with API keys
- MQTT telemetry ingestion
- Live dashboard with real-time data
- Remote setpoint control
- Device online/offline detection
- Basic device shadow

### Phase 2 — Multi-user + Roles (Weeks 5-8)
- Role-based access control (Admin/Technician/Viewer)
- Device assignment to users
- Alert rules (temp threshold, device offline)
- Historical data charts
- Audit logging
- Email notifications

### Phase 3 — Advanced SaaS (Weeks 9-16)
- Organization management (multi-tenant)
- OTA firmware updates
- Billing & subscription plans
- API key management & rate limiting
- White-label support
- Mobile-responsive PWA
- API monetization endpoints

---

## 📦 Tech Stack Versions

| Component | Technology | Version |
|-----------|-----------|---------|
| Frontend | Next.js | 15.x |
| Frontend UI | React | 19.x |
| Backend | Node.js | 22 LTS |
| Backend Framework | Fastify | 5.x |
| Database | PostgreSQL | 16 |
| Database Platform | Supabase | Latest |
| Cache | Redis | 7.x |
| MQTT Broker | EMQX Cloud | 5.x |
| MQTT Client (Node) | mqtt.js | 5.x |
| MQTT Client (ESP32) | PubSubClient | 2.8 |
| Auth | jsonwebtoken | 9.x |
| Password Hash | bcrypt | 5.x |
| ORM | Drizzle ORM | Latest |
| Validation | Zod | 3.x |
| WebSocket | @fastify/websocket | 11.x |
| Charts | Recharts | 2.x |
| Deployment | Vercel (FE) + Railway/Render (BE) | — |

---

## ⚠️ Risk Analysis

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| MQTT broker downtime | High - No real-time data | Low (EMQX SLA 99.9%) | Queue messages on device, retry logic |
| Database overload from telemetry | High - Platform unusable | Medium | Partitioning, batch inserts, retention policy |
| JWT token theft | High - Account compromise | Low | Short expiry, httpOnly cookies, refresh rotation |
| Device spoofing | High - False data | Medium | Unique API keys, MQTT ACL, payload validation |
| Cross-tenant data leak | Critical | Low | org_id scoped queries, row-level security |
| DDoS on API | High - Platform down | Medium | Rate limiting, Cloudflare, auto-scaling |
| ESP32 WiFi instability | Medium - Data gaps | High | Local buffering, reconnect logic, QoS 1 |
| Firmware update failure | High - Bricked device | Low in Phase 3 | Dual-partition OTA, rollback mechanism |
| Supabase free tier limits | Medium - Throttling | Medium | Monitor usage, upgrade plan, self-host fallback |
| Team scaling | Medium - Slow delivery | Medium | Modular architecture, clear API contracts |

---

## 🚢 Deployment Strategy

```
┌─────────────────────────────────────────────────────────────────┐
│                    DEPLOYMENT ARCHITECTURE                       │
│                                                                 │
│  ┌─────────────┐        ┌──────────────────────────┐           │
│  │   Vercel     │        │    Railway / Render      │           │
│  │   (Frontend) │        │    (Backend API)         │           │
│  │              │        │                          │           │
│  │  Next.js App │───────►│  Fastify + MQTT Client   │           │
│  │  SSR + Edge  │  API   │  Docker Container        │           │
│  └─────────────┘        └──────────┬───────────────┘           │
│                                     │                           │
│                          ┌──────────┼──────────┐               │
│                          │          │          │               │
│                    ┌─────▼──┐  ┌───▼────┐  ┌──▼──────────┐   │
│                    │Supabase│  │ Redis  │  │ EMQX Cloud  │   │
│                    │(PG+Auth│  │(Upstash│  │ (MQTT)      │   │
│                    │+Storage│  │  )     │  │             │   │
│                    └────────┘  └────────┘  └─────────────┘   │
│                                                                 │
│  CI/CD Pipeline (GitHub Actions):                               │
│  ├── Push to main → Auto-deploy to staging                     │
│  ├── Tag release → Deploy to production                        │
│  ├── Run tests before deploy                                   │
│  └── Database migrations via Drizzle                           │
└─────────────────────────────────────────────────────────────────┘
```
