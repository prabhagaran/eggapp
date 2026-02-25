import { drizzle } from 'drizzle-orm/postgres-js';
import postgres from 'postgres';
import config from '../config/index.js';
import * as schema from './schema.js';

const connectionString = config.databaseUrl;

if (!connectionString) {
    console.warn('[DB] DATABASE_URL not set. Database features will be unavailable.');
}

// Connection for queries (with pooling)
const queryClient = connectionString
    ? postgres(connectionString, {
        max: 10,
        idle_timeout: 20,
        connect_timeout: 10,
    })
    : null;

// Drizzle instance
const db = queryClient
    ? drizzle(queryClient, { schema })
    : null;

export { db, queryClient };
export default db;
