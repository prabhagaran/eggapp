import { Request, Response, NextFunction } from 'express';
import { AuthRequest } from './jwt.middleware';

export function requireOrgMatch(req: AuthRequest, res: Response, next: NextFunction) {
  const orgIdParam = req.params.orgId || req.body.orgId || req.query.orgId;
  if (!orgIdParam) return res.status(400).json({ error: 'Organization context required' });
  if (req.auth?.orgId !== orgIdParam) return res.status(403).json({ error: 'Org mismatch' });
  next();
}
