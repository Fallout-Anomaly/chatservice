# Base image pinned by DIGEST (Bun 1.3.14 alpine) to block tag-mutation / image-substitution.
# To update + re-pin:  docker pull oven/bun:1-alpine && docker inspect --format '{{index .RepoDigests 0}}' oven/bun:1-alpine
FROM oven/bun:1-alpine@sha256:5acc90a93e91ff07bf72aa90a7c9f0fa189765aec90b47bdbf2152d2196383c0
WORKDIR /app
ENV NODE_ENV=production

# Install ONLY from the committed lockfile — fails the build if package.json and bun.lock
# disagree, preventing dependency-confusion / silent version substitution at build time.
COPY package.json bun.lock ./
RUN bun install --frozen-lockfile --production

COPY index.js .

# Defense-in-depth: drop to the unprivileged 'bun' user even if compose 'user:' is removed.
USER bun
CMD ["bun", "index.js"]
