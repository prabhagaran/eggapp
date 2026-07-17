// Pure domain rules — no DB, no HTTP.
import { describe, expect, it } from "vitest";
import {
  assertHatchDateAllowed,
  assertTransition,
  candlingOutcome,
  classifyStorageAge,
  computeSchedule,
  fertilityPct,
  hatchDiscrepancy,
  hatchMetrics,
} from "../src/domain/incubation.js";

const chicken = { incubationDays: 21, lockdownOffsetDays: 3, candlingDays: [7, 14, 18] };
const DAY = 86_400_000;

describe("computeSchedule (BR-008)", () => {
  it("derives chicken lockdown day 18 and hatch day 21 from set date", () => {
    const setAt = new Date("2026-08-01T06:00:00Z");
    const s = computeSchedule(chicken, setAt);
    expect(s.lockdownAt.getTime()).toBe(setAt.getTime() + 18 * DAY);
    expect(s.expectedHatchAt.getTime()).toBe(setAt.getTime() + 21 * DAY);
    expect(s.candlingDays).toEqual([7, 14, 18]);
  });
});

describe("batch transitions (BR-001)", () => {
  it("allows the forward path and abort, forbids skipping and backward moves", () => {
    expect(() => assertTransition("planned", "incubating")).not.toThrow(); // setting optional
    expect(() => assertTransition("incubating", "lockdown")).not.toThrow();
    expect(() => assertTransition("lockdown", "aborted")).not.toThrow();
    expect(() => assertTransition("incubating", "hatching")).toThrow(); // skip
    expect(() => assertTransition("lockdown", "incubating")).toThrow(); // backward
    expect(() => assertTransition("aborted", "planned")).toThrow(); // terminal
    expect(() => assertTransition("completed", "closed")).not.toThrow();
  });
});

describe("storage age (BR-011)", () => {
  const collected = new Date("2026-08-01T00:00:00Z");
  const at = (days: number) => new Date(collected.getTime() + days * DAY);
  it("ok ≤7d, warning 8–14d, blocked >14d", () => {
    expect(classifyStorageAge(collected, at(7)).verdict).toBe("ok");
    expect(classifyStorageAge(collected, at(8)).verdict).toBe("warning");
    expect(classifyStorageAge(collected, at(14)).verdict).toBe("warning");
    expect(classifyStorageAge(collected, at(15)).verdict).toBe("blocked");
  });
});

describe("candling outcome (BR-003)", () => {
  it("viable = fertile + unsure; discrepancy against current viable", () => {
    const r = candlingOutcome({ fertile: 40, clear: 6, bloodRing: 2, unsure: 2 }, 50);
    expect(r.newViable).toBe(42);
    expect(r.discrepancy).toBe(0);
  });
  it("flags a count mismatch instead of absorbing it", () => {
    const r = candlingOutcome({ fertile: 40, clear: 6, bloodRing: 2, unsure: 2 }, 52);
    expect(r.discrepancy).toBe(-2);
  });
});

describe("hatch rules", () => {
  it("BR-008: rejects hatch >2 days before expected date, allows within window", () => {
    const expected = new Date("2026-08-22T06:00:00Z");
    expect(() => assertHatchDateAllowed(expected, new Date("2026-08-19T00:00:00Z"))).toThrow();
    expect(() => assertHatchDateAllowed(expected, new Date("2026-08-20T07:00:00Z"))).not.toThrow();
  });

  it("BR-003: reconciles hatch counts against viable-at-lockdown", () => {
    expect(hatchDiscrepancy({ hatched: 38, pippedDead: 1, deadInShell: 2, unhatched: 1 }, 42)).toBe(0);
    expect(hatchDiscrepancy({ hatched: 38, pippedDead: 1, deadInShell: 2, unhatched: 1 }, 44)).toBe(-2);
  });

  it("BR-015: metric definitions match domain-knowledge §4", () => {
    expect(fertilityPct(45, 50)).toBe(90);
    const m = hatchMetrics(50, 45, 38);
    expect(m.hatchOfSetPct).toBe(76);
    expect(m.hatchOfFertilePct).toBe(84.4);
    expect(hatchMetrics(0, null, 0)).toEqual({ hatchOfSetPct: null, hatchOfFertilePct: null });
  });
});
