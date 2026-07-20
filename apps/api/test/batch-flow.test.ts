// Full incubation lifecycle against a real DB. Skipped without DATABASE_URL.
import { describe, expect, it } from "vitest";
import { randomUUID } from "node:crypto";
import { buildApp } from "../src/app.js";

// /v1/setup only succeeds once per DB (subsequent calls 409) — every test
// case that needs a fresh farm+auth lives inside this single `it`, not as
// separate `it`s that would each try (and fail) to call /v1/setup again.

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
    const patch = (url: string, body: unknown) =>
      app.inject({ method: "PATCH", url, headers: authz, body: body as object });
    const del = (url: string) => app.inject({ method: "DELETE", url, headers: authz });
    const get = (url: string) => app.inject({ method: "GET", url, headers: authz });

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

    // Edit/delete are scoped to "planned" batches — tested against a
    // separate second batch (and second collection, since c1's 50 eggs
    // are already fully assigned to the first batch above).
    const inc2 = (
      await post(`/v1/farms/${farm.id}/incubators`, { name: "Flow Inc 2", capacity: 60 })
    ).json();
    const collB = (
      await post(`/v1/farms/${farm.id}/collections`, { collectedOn, count: 20 })
    ).json();
    const created2 = await post(`/v1/farms/${farm.id}/batches`, {
      incubatorId: inc.id,
      speciesId: "chicken",
      sources: [{ collectionId: collB.id, count: 10 }],
    });
    const batch2 = created2.json().batch;
    expect(batch2.viableCount).toBe(10);

    // Editing sources re-validates availability with this batch's own
    // prior source released first, so raising the count on the *same*
    // collection isn't blocked by its own old assignment.
    const edited = await patch(`/v1/farms/${farm.id}/batches/${batch2.id}`, {
      incubatorId: inc2.id,
      speciesId: "duck",
      sources: [{ collectionId: collB.id, count: 15 }],
    });
    expect(edited.statusCode).toBe(200);
    const editedBatch = edited.json().batch;
    expect(editedBatch.incubatorId).toBe(inc2.id);
    expect(editedBatch.speciesId).toBe("duck");
    expect(editedBatch.candlingDays).toEqual([7, 14, 25]); // BR-008 re-snapshot
    expect(editedBatch.viableCount).toBe(15);

    // Over-committing beyond what's now available (20 - 15 = 5 left) is rejected.
    const overCommit = await patch(`/v1/farms/${farm.id}/batches/${batch2.id}`, {
      sources: [{ collectionId: collB.id, count: 21 }],
    });
    expect(overCommit.statusCode).toBe(409);
    // ...and the failed attempt didn't leave the batch's sources empty
    // (the deleteMany-then-revalidate is transactional, so it rolled back).
    const stillFifteen = await get(`/v1/farms/${farm.id}/batches/${batch2.id}`);
    expect(stillFifteen.json().viableCount).toBe(15);

    // Delete releases the 15 eggs back to the collection's available pool.
    const deleted = await del(`/v1/farms/${farm.id}/batches/${batch2.id}`);
    expect(deleted.statusCode).toBe(204);
    expect((await get(`/v1/farms/${farm.id}/batches/${batch2.id}`)).statusCode).toBe(404);
    const collBAfter = await get(`/v1/farms/${farm.id}/collections`);
    const collBRow = collBAfter.json().find((c: { id: string }) => c.id === collB.id);
    expect(collBRow.availableCount).toBe(20);

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

    // Past "incubating" (lockdown+), edit/delete are rejected — candling
    // data is already load-bearing against the schedule by then.
    const editAfterSet = await patch(`/v1/farms/${farm.id}/batches/${batch.id}`, { speciesId: "duck" });
    expect(editAfterSet.statusCode).toBe(409);
    const deleteAfterSet = await del(`/v1/farms/${farm.id}/batches/${batch.id}`);
    expect(deleteAfterSet.statusCode).toBe(409);

    // "incubating" itself IS editable (correcting a mis-set batch while
    // live), just not deletable (real eggs are physically in a device —
    // abort instead). Exercised on a fresh batch so it doesn't disturb the
    // chicken candling flow above.
    const collC = (await post(`/v1/farms/${farm.id}/collections`, { collectedOn, count: 20 })).json();
    const created3 = await post(`/v1/farms/${farm.id}/batches`, {
      incubatorId: inc.id,
      speciesId: "chicken",
      sources: [{ collectionId: collC.id, count: 5 }],
    });
    const batch3 = created3.json().batch;
    const setBatch3 = await post(`/v1/farms/${farm.id}/batches/${batch3.id}/set`, {});
    const lockdownBeforeEdit = setBatch3.json().lockdownAt;

    const editIncubating = await patch(`/v1/farms/${farm.id}/batches/${batch3.id}`, { speciesId: "duck" });
    expect(editIncubating.statusCode).toBe(200);
    const editedIncubating = editIncubating.json().batch;
    expect(editedIncubating.speciesId).toBe("duck");
    expect(editedIncubating.candlingDays).toEqual([7, 14, 25]); // BR-008 re-snapshot
    expect(editedIncubating.lockdownAt).not.toBe(lockdownBeforeEdit); // schedule recomputed off existing setAt

    const deleteIncubating = await del(`/v1/farms/${farm.id}/batches/${batch3.id}`);
    expect(deleteIncubating.statusCode).toBe(409); // must abort, not delete, while live

    // Aborted IS deletable (terminal, no hardware using it).
    const abortBatch3 = await post(`/v1/farms/${farm.id}/batches/${batch3.id}/status`, {
      status: "aborted",
      reason: "test cleanup",
    });
    expect(abortBatch3.statusCode).toBe(200);
    const deleteAborted = await del(`/v1/farms/${farm.id}/batches/${batch3.id}`);
    expect(deleteAborted.statusCode).toBe(204);

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
