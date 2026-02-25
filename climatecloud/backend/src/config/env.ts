import dotenv from 'dotenv';
import path from 'path';

const env = process.env.NODE_ENV || 'development';
const envFile = env === 'production' ? '.env.production' : '.env.development';
dotenv.config({ path: path.resolve(process.cwd(), envFile) });

export const config = {
  nodeEnv: env,
  port: Number(process.env.PORT || 3001),
  jwt: {
    accessSecret: process.env.JWT_ACCESS_SECRET || 'dev-access',
    refreshSecret: process.env.JWT_REFRESH_SECRET || 'dev-refresh',
    accessExpiry: process.env.JWT_ACCESS_EXPIRY || '15m',
    refreshExpiry: process.env.JWT_REFRESH_EXPIRY || '7d'
  },
  dbUrl: process.env.DATABASE_URL || '',
  mqtt: {
    brokerUrl: process.env.MQTT_BROKER_URL || ''
  }
};

export default config;
