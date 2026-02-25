import { Request, Response, NextFunction } from 'express';
import { AuthRequest } from './jwt.middleware';

export const requireRoles = (...allowed: string[]) => {
  return (req: AuthRequest, res: Response, next: NextFunction) => {
    const role = req.auth?.role;
    if (!role || !allowed.includes(role)) return res.status(403).json({ error: 'Forbidden' });
    next();
  };
};
