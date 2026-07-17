const DEV_SECRET = "dev-insecure-secret";

export const config = {
  port: Number(process.env.PORT ?? 3001),
  jwtSecret: process.env.JWT_SECRET ?? DEV_SECRET,
  accessTokenTtl: "15m",
  refreshTokenTtl: "30d",
  isProd: process.env.NODE_ENV === "production",
};

if (config.isProd && config.jwtSecret === DEV_SECRET) {
  throw new Error("JWT_SECRET must be set in production");
}
