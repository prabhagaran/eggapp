import app from './app';
import { config } from './config/env';
import { logger } from './utils/logger';

const port = config.port;
app.listen(port, () => {
  logger.info(`API listening on ${port}`);
});
