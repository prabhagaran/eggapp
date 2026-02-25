import { Request, Response, NextFunction } from 'express';
import { verifyAccess } from '../services/token.service';
import { logger } from '../utils/logger';

export interface AuthRequest extends Request {
  auth?: any
}

export function jwtMiddleware(req: AuthRequest, res: Response, next: NextFunction) {
  const auth = req.headers.authorization as string;
  if (!auth || !auth.startsWith('Bearer ')) return res.status(401).json({ error: 'Missing token' });
  const token = auth.slice(7);
  try {
    const decoded = verifyAccess(token);
    req.auth = decoded;
    next();
  } catch (err) {
    logger.warn('JWT verify failed', err as any);
    return res.status(401).json({ error: 'Invalid token' });
  }
}
