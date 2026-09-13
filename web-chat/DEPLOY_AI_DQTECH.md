# DQ AI deployment: ai.dqtech.cloud

This app runs as a Node service on `127.0.0.1:3001`. Apache handles the public hostname and forwards requests to it. Text chat is forwarded to Voice AI at `127.0.0.1:8000`; Voice AI owns Ollama, semantic retrieval and Qdrant.

## First deployment

Run these commands from the server:

```bash
cd "/home/dq/Local AI/web-chat"
npm ci
npm run build

sudo cp deploy/dq-ai.service /etc/systemd/system/dq-ai.service
sudo a2enmod proxy proxy_http headers rewrite
sudo cp deploy/ai.dqtech.cloud.conf /etc/apache2/sites-available/ai.dqtech.cloud.conf
sudo a2ensite ai.dqtech.cloud.conf
sudo apache2ctl configtest
sudo systemctl daemon-reload
sudo systemctl enable --now dq-ai
sudo systemctl reload apache2
```

Check the local app and Apache route:

```bash
curl -fsS http://127.0.0.1:3001/api/models
curl -i -H 'Host: ai.dqtech.cloud' http://127.0.0.1/
sudo systemctl status dq-ai --no-pager
```

## DNS and HTTPS

Create an `A` record at your DNS provider:

```text
ai.dqtech.cloud  A  <this-server-public-ip>
```

Wait for DNS to resolve, then issue the certificate:

```bash
getent hosts ai.dqtech.cloud
sudo certbot --apache -d ai.dqtech.cloud
```

Certbot will create the HTTPS virtual host and redirect HTTP to HTTPS. Verify with:

```bash
curl -I https://ai.dqtech.cloud
```

If Cloudflare is used, keep the record proxied only after the origin is reachable on port 80/443. Ollama must not be exposed publicly.

## Updates and logs

```bash
cd "/home/dq/Local AI/web-chat"
npm ci
npm run build
sudo systemctl restart dq-ai
sudo journalctl -u dq-ai -n 100 --no-pager
sudo tail -f /var/log/apache2/ai.dqtech.cloud-error.log
```

The app uses `OLLAMA_MODEL=dq-assistant:latest` for its model selector and sends requests to `VOICE_AI_URL=http://127.0.0.1:8000`. Build the collection with `voice_ai_server/index_knowledge.py` after every knowledge change. Configure `VOICE_AI_API_KEY` if Voice AI requires an API key. Browser conversation history stays in each user's local browser. The demo login password is stored in the frontend source and is not a multi-user authentication system; add real server-side authentication before exposing this publicly.
