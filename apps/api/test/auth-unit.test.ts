// Tests that must pass with no database available: validation and auth
// gates all reject before any Prisma call.
import { describe, expect, it } from "vitest";
import { buildApp } from "../src/app.js";

describe("auth & validation (no DB)", () => {
  it("rejects /v1/me without a token", async () => {
    const app = buildApp();
    const res = await app.inject({ method: "GET", url: "/v1/me" });
    expect(res.statusCode).toBe(401);
    expect(res.json().error.code).toBe("unauthorized");
    await app.close();
  });

  it("rejects a refresh token used as an access token", async () => {
    const app = buildApp();
    await app.ready();
    const refresh = app.jwt.sign({ sub: "u1", type: "refresh", ver: 0 });
    const res = await app.inject({
      method: "GET",
      url: "/v1/me",
      headers: { authorization: `Bearer ${refresh}` },
    });
    expect(res.statusCode).toBe(401);
    await app.close();
  });

  it("rejects an access token on /v1/auth/refresh before touching the DB", async () => {
    const app = buildApp();
    await app.ready();
    const access = app.jwt.sign({ sub: "u1", type: "access" });
    const res = await app.inject({
      method: "POST",
      url: "/v1/auth/refresh",
      body: { refreshToken: access },
    });
    expect(res.statusCode).toBe(401);
    expect(res.json().error.message).toMatch(/refresh/i);
    await app.close();
  });

  it("400s setup with an invalid body (zod, before DB)", async () => {
    const app = buildApp();
    const res = await app.inject({
      method: "POST",
      url: "/v1/setup",
      body: { email: "not-an-email", password: "short" },
    });
    expect(res.statusCode).toBe(400);
    expect(res.json().error.code).toBe("validation");
    await app.close();
  });

  it("round-trips a signed token", async () => {
    const app = buildApp();
    await app.ready();
    const token = app.jwt.sign({ sub: "user-1", type: "access" });
    const decoded = app.jwt.verify<{ sub: string; type: string }>(token);
    expect(decoded.sub).toBe("user-1");
    expect(decoded.type).toBe("access");
    await app.close();
  });
});
