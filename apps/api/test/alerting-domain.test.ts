// Pure alerting rules (BR-006) — no DB, no HTTP.
import { describe, expect, it } from "vitest";
import { classify, effectiveBounds } from "../src/domain/alerting.js";

const chicken = {
  tempSetC: 37.6,
  tempMinC: 36.1,
  tempMaxC: 38.9,
  humidityIncubMinPct: 50,
  humidityIncubMaxPct: 55,
  humidityLockMinPct: 65,
  humidityLockMaxPct: 70,
};

describe("effectiveBounds — temperature (BR-006)", () => {
  it("warns at ±0.3°C from setpoint, critical at species min/max", () => {
    const b = effectiveBounds(chicken, "incubating", "temp_c");
    expect(b).toEqual({ warnMin: 37.3, warnMax: 37.9, critMin: 36.1, critMax: 38.9 });
  });

  it("stage does not affect temperature bounds", () => {
    expect(effectiveBounds(chicken, "incubating", "temp_c")).toEqual(
      effectiveBounds(chicken, "lockdown", "temp_c"),
    );
  });
});

describe("effectiveBounds — humidity (BR-006 stage-aware)", () => {
  it("uses the incubation range as warning bounds, ±10pp for critical", () => {
    const b = effectiveBounds(chicken, "incubating", "humidity_pct");
    expect(b).toEqual({ warnMin: 50, warnMax: 55, critMin: 40, critMax: 65 });
  });

  it("uses the lockdown range — different from incubation (this is the point of BR-006)", () => {
    const b = effectiveBounds(chicken, "lockdown", "humidity_pct");
    expect(b).toEqual({ warnMin: 65, warnMax: 70, critMin: 55, critMax: 80 });
  });
});

describe("classify", () => {
  const bounds = { warnMin: 37.3, warnMax: 37.9, critMin: 36.1, critMax: 38.9 };

  it("ok inside warning bounds", () => {
    expect(classify(37.6, bounds)).toBe("ok");
  });
  it("warning just outside warning bounds, still inside critical", () => {
    expect(classify(38.0, bounds)).toBe("warning");
    expect(classify(37.0, bounds)).toBe("warning");
  });
  it("critical outside critical bounds", () => {
    expect(classify(39.0, bounds)).toBe("critical");
    expect(classify(35.0, bounds)).toBe("critical");
  });
  it("boundary values are inclusive of the tighter band (not a breach)", () => {
    expect(classify(37.3, bounds)).toBe("ok");
    expect(classify(36.1, bounds)).toBe("warning");
  });
  it("null (sensor fault) is never a breach — that's a device concern, not environmental", () => {
    expect(classify(null, bounds)).toBe("ok");
  });
});
