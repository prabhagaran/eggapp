import { Router } from 'express';
import { prisma } from '../prisma/client';
import { jwtMiddleware } from '../middleware/jwt.middleware';

const router = Router();

router.post('/', jwtMiddleware, async (req:any, res, next) => {
  try {
    if (req.auth?.type !== 'device') return res.status(403).json({ error: 'Device token required' });
    const deviceId = req.auth.sub as string;
    const payload = req.body;
    await prisma.telemetry.create({ data: { deviceId, payload: payload as any, timestamp: new Date() } });
    res.status(201).send({ ok: true });
  } catch (err) { next(err); }
});

export default router;
