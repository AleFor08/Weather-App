FROM docker.n8n.io/n8nio/n8n

WORKDIR /n8n
COPY package*.json ./
RUN npm install
COPY . .