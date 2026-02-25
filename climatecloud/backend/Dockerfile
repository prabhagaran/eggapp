FROM node:22-alpine

WORKDIR /app

# Copy package files
COPY package.json ./

# Install dependencies (include dev deps so pino-pretty is available in dev)
RUN npm install

# Copy source
COPY src/ ./src/
COPY drizzle.config.js ./

# Expose port
EXPOSE 3001

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s \
  CMD wget --no-verbose --tries=1 --spider http://localhost:3001/health || exit 1

# Start
CMD ["node", "src/server.js"]
