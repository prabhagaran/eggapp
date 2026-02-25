# 🌩 ClimateCloud — IoT SaaS Platform

> A production-grade, multi-tenant IoT cloud platform for climate control devices.
> Built from scratch as a commercial-ready alternative to Blynk, Home Assistant, and OpenHAB.

---

## 🎯 What is ClimateCloud?

ClimateCloud is a full-stack IoT SaaS platform designed for:
- **Egg incubators**, climate chambers, thermostats, and generic IoT devices
- **Real-time monitoring** with WebSocket live updates
- **Remote control** via MQTT commands
- **Device shadow** synchronization (AWS IoT Core-style)
- **Multi-tenant** architecture with organization isolation
- **Role-based access** (Owner, Admin, Technician, Viewer)

---

## 🏗 Architecture

```
Frontend (Next.js)  ←→  Backend API (Fastify)  ←→  MQTT Broker (EMQX)
       ↕                        ↕                         ↕
   WebSocket              PostgreSQL + Redis          ESP32 Devices
```

📖 [Full Architecture Documentation](./ARCHITECTURE.md)
📖 [API Reference](./API_REFERENCE.md)

---

## 📦 Project Structure

```
climatecloud/
├── ARCHITECTURE.md          # System architecture & diagrams
├── API_REFERENCE.md         # REST API documentation
├── docker-compose.yml       # Local dev environment
│
├── backend/                 # Node.js Fastify API Server
│   ├── src/
│   │   ├── config/          # Environment configuration
│   │   ├── db/
│   │   │   ├── schema.js    # Drizzle ORM schema (all tables)
│   │   │   └── index.js     # DB connection
│   │   ├── modules/
│   │   │   ├── auth.js      # JWT auth (register/login/refresh)
│   │   │   ├── devices.js   # Device CRUD + API key gen
│   │   │   ├── telemetry.js # Data ingestion + queries
│   │   │   ├── commands.js  # Remote commands via MQTT
│   │   │   ├── shadow.js    # Device shadow sync
│   │   │   ├── admin.js     # User management
│   │   │   └── audit.js     # Audit logging
│   │   ├── services/
│   │   │   └── mqtt.js      # MQTT broker integration
│   │   └── server.js        # Main entry point
│   ├── drizzle.config.js
│   ├── Dockerfile
│   └── package.json
│
├── frontend/                # Next.js 15 Web Application
│   ├── src/
│   │   ├── app/
│   │   │   ├── layout.js    # Root layout
│   │   │   ├── page.js      # Landing redirect
│   │   │   ├── globals.css  # Design system
│   │   │   ├── login/       # Login page
│   │   │   ├── register/    # Registration page
│   │   │   └── dashboard/
│   │   │       ├── layout.js      # Sidebar + auth guard
│   │   │       ├── page.js        # Dashboard overview
│   │   │       ├── devices/
│   │   │       │   ├── page.js    # Device list + register
│   │   │       │   └── [id]/
│   │   │       │       └── page.js # Device detail + control
│   │   │       └── admin/
│   │   │           └── page.js    # User management
│   │   └── lib/
│   │       ├── api.js         # API client with JWT refresh
│   │       ├── auth-context.js # React auth provider
│   │       └── useWebSocket.js # Real-time WebSocket hook
│   └── package.json
│
└── firmware/                # ESP32 MQTT Module
    ├── climatecloud_mqtt.h   # MQTT client header
    ├── climatecloud_mqtt.cpp # MQTT client implementation
    └── integration_example.ino  # How to integrate
```

---

## 🚀 Quick Start

### Prerequisites
- Node.js 22+
- PostgreSQL 16 (or Supabase)
- MQTT Broker (EMQX Cloud free tier)

### 1. Backend Setup
```bash
cd climatecloud/backend
cp .env.example .env     # Configure your credentials
npm install
npm run db:push           # Push schema to database
npm run dev               # Start dev server on :3001
```

### 2. Frontend Setup
```bash
cd climatecloud/frontend
npm install
npm run dev               # Start Next.js on :3000
```

### 3. Docker (Alternative)
```bash
cd climatecloud
docker-compose up -d      # Starts API + PostgreSQL + Redis
cd frontend && npm run dev
```

### 4. ESP32 Firmware
1. Register a device via the dashboard
2. Copy MQTT credentials
3. Add `climatecloud_mqtt.h/.cpp` to your Arduino project
4. Follow `integration_example.ino` for integration steps
5. Flash and connect!

---

## 🔑 Key Features

| Feature | Status |
|---------|--------|
| User registration & login | ✅ Phase 1 |
| JWT with refresh token rotation | ✅ Phase 1 |
| Device registration with API keys | ✅ Phase 1 |
| MQTT telemetry ingestion | ✅ Phase 1 |
| Real-time WebSocket dashboard | ✅ Phase 1 |
| Device online/offline detection | ✅ Phase 1 |
| Remote command execution | ✅ Phase 1 |
| Device shadow (desired/reported) | ✅ Phase 1 |
| Role-based access control | ✅ Phase 2 |
| Admin user management | ✅ Phase 2 |
| Historical data charts | ✅ Phase 1 |
| Audit logging | ✅ Phase 1 |
| Alert rules | 📋 Phase 2 |
| OTA firmware updates | 📋 Phase 3 |
| Billing & subscriptions | 📋 Phase 3 |
| Multi-org isolation | 📋 Phase 3 |

---

## 📡 MQTT Topics

```
climatecloud/device/{device_uid}/telemetry      # Device → Cloud
climatecloud/device/{device_uid}/status          # Device → Cloud (retained)
climatecloud/device/{device_uid}/command         # Cloud → Device
climatecloud/device/{device_uid}/command/ack     # Device → Cloud
climatecloud/device/{device_uid}/shadow/update   # Device → Cloud
climatecloud/device/{device_uid}/shadow/delta    # Cloud → Device
```

---

## 🛡 Security

- **HTTPS/TLS** everywhere
- **bcrypt** password hashing (12 rounds)
- **JWT** with 15-min access + 7-day refresh tokens
- **Token rotation** on refresh
- **MQTT ACL** per device
- **Rate limiting** (100 req/min)
- **Org-scoped** data isolation
- **Audit logs** for all sensitive operations

---

## 📜 License

MIT License — Build your commercial IoT product with confidence.
