import express from 'express';
import morgan from 'morgan';
import authRoutes from './controllers/auth.controller';
import deviceRoutes from './controllers/device.controller';
import telemetryRoutes from './controllers/telemetry.controller';
import { errorHandler } from './middleware/error.middleware';
import { config } from './config/env';

const app = express();
app.use(express.json());
if (config.nodeEnv !== 'production') app.use(morgan('dev'));

app.use('/api/v1/auth', authRoutes);
app.use('/api/v1/devices', deviceRoutes);
app.use('/api/v1/telemetry', telemetryRoutes);

app.use(errorHandler);

export default app;
