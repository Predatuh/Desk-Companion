FROM node:20-alpine
WORKDIR /app
COPY relay_server/package.json ./
RUN npm install --production
COPY relay_server/ .
CMD ["node", "server.js"]
