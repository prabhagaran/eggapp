import winston from 'winston';
import { config } from '../config/env';

const level = config.nodeEnv === 'production' ? 'info' : 'debug';

export const logger = winston.createLogger({
  level,
  format: winston.format.combine(
    winston.format.timestamp(),
    winston.format.errors({ stack: true }),
    winston.format.json()
  ),
  transports: [
    new winston.transports.Console({
      format: config.nodeEnv === 'production'
        ? winston.format.json()
        : winston.format.combine(winston.format.colorize(), winston.format.simple())
    })
  ]
});

export default logger;
