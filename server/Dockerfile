# Base image pinned by DIGEST (Bun 1.3.14 alpine) to block tag-mutation / image-substitution.
# This service has ZERO dependencies (pure Bun WebSocket relay) — no install step, no supply-chain surface.
FROM oven/bun:1-alpine@sha256:5acc90a93e91ff07bf72aa90a7c9f0fa189765aec90b47bdbf2152d2196383c0
WORKDIR /app
ENV NODE_ENV=production
COPY index.js .
USER bun
EXPOSE 3000
CMD ["bun", "index.js"]
