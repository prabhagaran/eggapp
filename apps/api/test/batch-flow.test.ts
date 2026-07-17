// Full incubation lifecycle against a real DB. Skipped without DATABASE_URL.
import { describe, expect, it } from "vitest";
import { randomUUID } from "node:crypto";
import { buildApp } from "../src/app.js";

const hasDb = Boolean(process.env.DATABASE_URL);

describe.runIf(hasDb)("collection → batch → candle → hatch flow (DB)", () => {
  // 30s: ~15 sequential round-trips against a live remote DB — not a hang.
  it("runs the full lifecycle with BR-002/003/008/010/015 enforced", async () => {
    const app = buildApp();
    const suffix = Date.now();
    const email = `flow-${suffix}@test.local`;

    // Bootstrap or reuse: setup, else login path is unavailable in a fresh
    // test env — this suite assumes it can create its own owner+farm.
    const setupRes = await app.inject({
      method: "POST",
      url: "/v1/setup",
      body: {
        ownerName: "Flow",
        email,
        password: "correct-horse-battery",
        farmName: `Flow Farm ${suffix}`,
        timezone: "Asia/Kolkata",
      },
    });
    if (setupRes.statusCode === 409) {
      await app.close();
      return; // already-initialized DB — covered by integration.test.ts
    }
    const { accessToken, farm } = setupRes.json();
    const authz = { authorization: `Bearer ${accessToken}` };
    const post = (url: string, body: unknown) =>
      app.inject({ method: "POST", url, headers: authz, body: body as object });

    const inc = (
      await post(`/v1/farms/${farm.id}/incubators`, { name: "Flow Inc", capacity: 60 })
    ).json();

    // Collection with idempotent replay (BR-010)
    const clientId = randomUUID();
    const collectedOn = new Date().toISOString();
    const c1 = await post(`/v1/farms/${farm.id}/collections`, { collectedOn, count: 50, clientId });
    expect(c1.statusCode).toBe(201);
    const c2 = await post(`/v1/farms/${farm.id}/collections`, { collectedOn, count: 50, clientId });
    expect(c2.statusCode).toBe(200); // replay, not duplicate
    expect(c2.json().id).toBe(c1.json().id);

    // Batch creation snapshots the chicken schedule
    const created = await post(`/v1/farms/${farm.id}/batches`, {
      incubatorId: inc.id,
      speciesId: "chicken",
      sources: [{ collectionId: c1.json().id, count: 50 }],
    });
    expect(created.statusCode).toBe(201);
    const batch = created.json().batch;
    expect(batch.candlingDays).toEqual([7, 14, 18]);
    expect(batch.viableCount).toBe(50);

    // Candling before set (still planned) must fail BR-002
    const early = await post(`/v1/farms/${farm.id}/batches/${batch.id}/candlings`, {
      dayNo: 7,
      candledAt: new Date().toISOString(),
      fertile: 45,
      clear: 4,
      bloodRing: 1,
      unsure: 0,
    });
    expect(early.statusCode).toBe(409);

    // Set (schedule computed), then candle day 7
    const set = await post(`/v1/farms/${farm.id}/batches/${batch.id}/set`, {});
    expect(set.statusCode).toBe(200);
    expect(set.json().expectedHatchAt).toBeTruthy();

    const candled = await post(`/v1/farms/${farm.id}/batches/${batch.id}/candlings`, {
      dayNo: 7,
      candledAt: new Date().toISOString(),
      fertile: 45,
      clear: 4,
      bloodRing: 1,
      unsure: 0,
    });
    expect(candled.statusCode).toBe(201);
    expect(candled.json().batch.viableCount).toBe(45);
    expect(candled.json().batch.fertilityPct).toBe(90); // 45/50, BR-015 at first candle

    // Hatch too early (BR-008) → 409
    await post(`/v1/farms/${farm.id}/batches/${batch.id}/status`, { status: "lockdown" });
    const tooEarly = await post(`/v1/farms/${farm.id}/batches/${batch.id}/hatch`, {
      hatchedAt: new Date().toISOString(),
      hatched: 40,
      pippedDead: 2,
      deadInShell: 2,
      unhatched: 1,
    });
    expect(tooEarly.statusCode).toBe(409);

    // Hatch at expected date with reconciling counts → completed + metrics
    const hatchedAt = new Date(Date.parse(set.json().expectedHatchAt)).toISOString();
    const hatched = await post(`/v1/farms/${farm.id}/batches/${batch.id}/hatch`, {
      hatchedAt,
      hatched: 40,
      pippedDead: 2,
      deadInShell: 2,
      unhatched: 1,
    });
    expect(hatched.statusCode).toBe(201);
    const done = hatched.json().batch;
    expect(done.status).toBe("completed");
    expect(done.hatchOfSetPct).toBe(80); // 40/50
    expect(done.hatchOfFertilePct).toBe(88.9); // 40/45

    await app.close();
  }, 30000);
});
