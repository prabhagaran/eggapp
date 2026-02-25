import { Router } from 'express';
import { prisma } from '../prisma/client';
import { hash, compare } from '../utils/hash';
import { signAccess } from '../services/token.service';
import { jwtMiddleware } from '../middleware/jwt.middleware';
import { requireRoles } from '../middleware/rbac.middleware';

const router = Router();

router.post('/register', jwtMiddleware, requireRoles('OWNER','ADMIN'), async (req:any,res,next) => {
  try {
    const { name, type } = req.body;
    const orgId = req.auth!.orgId as string;
    const deviceId = `dev_${Math.random().toString(36).slice(2,10)}`;
    const deviceSecretPlain = Math.random().toString(36).slice(2,18);
    const deviceSecretHash = await hash(deviceSecretPlain);
    const device = await prisma.device.create({ data: { deviceId, deviceSecret: deviceSecretHash, name, type, orgId } });
    res.status(201).json({ device: { id: device.id, deviceId, name, type }, credentials: { deviceSecret: deviceSecretPlain } });
  } catch (err) { next(err); }
});

router.post('/auth', async (req,res,next) => {
  try {
    const { deviceId, deviceSecret } = req.body;
    const device = await prisma.device.findUnique({ where: { deviceId } });
    if (!device) return res.status(401).json({ error: 'Invalid device' });
    const ok = await compare(deviceSecret, device.deviceSecret);
    if (!ok) return res.status(401).json({ error: 'Invalid device' });
    const token = signAccess({ sub: device.id, orgId: device.orgId, type: 'device' });
    res.json({ accessToken: token });
  } catch (err) { next(err); }
});

export default router;
