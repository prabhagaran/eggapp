import { Router } from 'express';
import { prisma } from '../prisma/client';
import { hash, compare } from '../utils/hash';
import { signAccess, signRefresh, verifyRefresh } from '../services/token.service';

const router = Router();

router.post('/register', async (req, res, next) => {
  try {
    const { email, password, firstName, lastName, orgName } = req.body;
    const org = await prisma.organization.create({ data: { name: orgName, slug: `${orgName}-${Date.now()}` } });
    const passwordHash = await hash(password);
    const user = await prisma.user.create({ data: { email: email.toLowerCase(), passwordHash, firstName, lastName, role: 'OWNER', orgId: org.id } });
    const accessToken = signAccess({ sub: user.id, email: user.email, orgId: org.id, role: user.role });
    const refreshToken = signRefresh({ sub: user.id, type: 'refresh' });
    await prisma.user.update({ where: { id: user.id }, data: { refreshHash: await hash(refreshToken) } });
    res.status(201).json({ user: { id: user.id, email: user.email, firstName, lastName, role: user.role, orgId: org.id }, accessToken, refreshToken });
  } catch (err) { next(err); }
});

router.post('/login', async (req, res, next) => {
  try {
    const { email, password } = req.body;
    const user = await prisma.user.findUnique({ where: { email: email.toLowerCase() } });
    if (!user) return res.status(401).json({ error: 'Invalid credentials' });
    const ok = await compare(password, user.passwordHash);
    if (!ok) return res.status(401).json({ error: 'Invalid credentials' });
    const accessToken = signAccess({ sub: user.id, email: user.email, orgId: user.orgId, role: user.role });
    const refreshToken = signRefresh({ sub: user.id, type: 'refresh' });
    await prisma.user.update({ where: { id: user.id }, data: { refreshHash: await hash(refreshToken) } });
    res.json({ user: { id: user.id, email: user.email, firstName: user.firstName, lastName: user.lastName, role: user.role, orgId: user.orgId }, accessToken, refreshToken });
  } catch (err) { next(err); }
});

router.post('/refresh', async (req, res, next) => {
  try {
    const { refreshToken } = req.body;
    if (!refreshToken) return res.status(400).json({ error: 'Refresh token required' });
    const decoded:any = verifyRefresh(refreshToken);
    const user = await prisma.user.findUnique({ where: { id: decoded.sub } });
    if (!user) return res.status(401).json({ error: 'Invalid refresh' });
    const valid = await compare(refreshToken, user.refreshHash || '');
    if (!valid) return res.status(401).json({ error: 'Refresh revoked' });
    const newAccess = signAccess({ sub: user.id, email: user.email, orgId: user.orgId, role: user.role });
    const newRefresh = signRefresh({ sub: user.id, type: 'refresh' });
    await prisma.user.update({ where: { id: user.id }, data: { refreshHash: await hash(newRefresh) } });
    res.json({ accessToken: newAccess, refreshToken: newRefresh });
  } catch (err) { next(err); }
});

export default router;
