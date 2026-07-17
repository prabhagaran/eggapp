// Full-flow tests against a real database. Skipped when DATABASE_URL is
// absent (e.g. before Supabase setup); run locally with packages/db/.env
// values exported, or in CI with a DB secret.
import { describe, expect, it } from "vitest";
import { buildApp } from "../src/app.js";

const hasDb = Boolean(process.env.DATABASE_URL);

describe.runIf(hasDb)("first-run → auth → incubator → device flow (DB)", () => {
  const suffix = Date.now();
  const email = `owner-${suffix}@test.local`;
  const password = "correct-horse-battery";

  it("runs setup, logs in, creates and binds resources with RBAC intact", async () => {
    const app = buildApp();

    // First-run setup (or already initialized from a previous run)
    const setupRes = await app.inject({
      method: "POST",
      url: "/v1/setup",
      body: {
        ownerName: "Owner",
        email,
        password,
        farmName: `Test Farm ${suffix}`,
        timezone: "Asia/Kolkata",
      },
    });
    expect([201, 409]).toContain(setupRes.statusCode);
    if (setupRes.statusCode === 409) {
      // DB already initialized — can't continue meaningfully in this run.
      await app.close();
      return;
    }
    const { accessToken, refreshToken, farm } = setupRes.json();
    const authz = { authorization: `Bearer ${accessToken}` };

    // Login + refresh
    const loginRes = await app.inject({
      method: "POST",
      url: "/v1/auth/login",
      body: { email, password },
    });
    expect(loginRes.statusCode).toBe(200);
    const refreshRes = await app.inject({
      method: "POST",
      url: "/v1/auth/refresh",
      body: { refreshToken },
    });
    expect(refreshRes.statusCode).toBe(200);

    // Species reference is reachable
    const speciesRes = await app.inject({ method: "GET", url: "/v1/species", headers: authz });
    expect(speciesRes.statusCode).toBe(200);

    // Incubator + device lifecycle (BR-007)
    const incRes = await app.inject({
      method: "POST",
      url: `/v1/farms/${farm.id}/incubators`,
      headers: authz,
      body: { name: "Incubator A", capacity: 56, defaultSpeciesId: "chicken" },
    });
    expect(incRes.statusCode).toBe(201);
    const incubator = incRes.json();

    const devRes = await app.inject({
      method: "POST",
      url: `/v1/farms/${farm.id}/devices`,
      headers: authz,
      body: { hardwareId: `AA:BB:CC:${suffix}`, name: "ESP32 #1" },
    });
    expect(devRes.statusCode).toBe(201);
    const device = devRes.json();

    const bindRes = await app.inject({
      method: "POST",
      url: `/v1/farms/${farm.id}/devices/${device.id}/bind`,
      headers: authz,
      body: { incubatorId: incubator.id },
    });
    expect(bindRes.statusCode).toBe(200);

    // Second bind attempt to the same incubator from a fresh device → 409
    const dev2Res = await app.inject({
      method: "POST",
      url: `/v1/farms/${farm.id}/devices`,
      headers: authz,
      body: { hardwareId: `DD:EE:FF:${suffix}` },
    });
    const bind2Res = await app.inject({
      method: "POST",
      url: `/v1/farms/${farm.id}/devices/${dev2Res.json().id}/bind`,
      headers: authz,
      body: { incubatorId: incubator.id },
    });
    expect(bind2Res.statusCode).toBe(409);

    // Scoping: a random farm id must 404, not leak
    const foreignRes = await app.inject({
      method: "GET",
      url: `/v1/farms/00000000-0000-4000-8000-000000000000/incubators`,
      headers: authz,
    });
    expect(foreignRes.statusCode).toBe(404);

    await app.close();
  });
});
