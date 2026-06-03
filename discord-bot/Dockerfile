FROM oven/bun:1-alpine
WORKDIR /app
COPY package.json ./
RUN bun install
COPY index.js .
CMD ["bun", "index.js"]
