CREATE EXTENSION IF NOT EXISTS pgcrypto;
BEGIN;
WITH o AS (
  INSERT INTO organizations (name, slug, created_at, updated_at)
  VALUES ('My Farm Co', 'my-farm-co-1', now(), now())
  RETURNING id
)
INSERT INTO users (email, password_hash, first_name, last_name, org_id, role, is_active, created_at, updated_at)
VALUES ('user@example.com', crypt('securepassword', gen_salt('bf', 12)), 'John', 'Doe', (SELECT id FROM o), 'owner', true, now(), now())
RETURNING id,email;
COMMIT;
