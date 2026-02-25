import jwt from 'jsonwebtoken';
import { config } from '../config/env';

export function signAccess(payload: object) {
  return jwt.sign(payload, config.jwt.accessSecret, { expiresIn: config.jwt.accessExpiry });
}
export function signRefresh(payload: object) {
  return jwt.sign(payload, config.jwt.refreshSecret, { expiresIn: config.jwt.refreshExpiry });
}
export function verifyAccess(token: string) {
  return jwt.verify(token, config.jwt.accessSecret) as any;
}
export function verifyRefresh(token: string) {
  return jwt.verify(token, config.jwt.refreshSecret) as any;
}
