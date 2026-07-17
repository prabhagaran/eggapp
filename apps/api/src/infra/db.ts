import { PrismaClient } from "@prisma/client";

// Lazy singleton: constructing at import time would require DATABASE_URL
// even for code paths that never touch the DB (health, token verification).
let client: PrismaClient | undefined;

export function getPrisma(): PrismaClient {
  client ??= new PrismaClient();
  return client;
}
