// Host for Claude Design's asset export, run by tools/design-export.sh.
//
// The design project is three.js source (docs/design/models/*.js) and its export
// page needs WebGL — the skyboxes are shaders baked to a texture — so the
// models are built by a real browser. This serves that page and catches what
// it produces, writing each file straight into the repo:
//   GET  /export.html     -> tools/design-export/export.html (the page)
//   GET  /vendor/<path>   -> tools/design-export/node_modules/<path> (three.js, pinned)
//   GET  /<path>          -> docs/design/<path> (models, icons, tokens, design docs)
//   PUT  /out/<path>      -> <repo>/<path>  (game/assets/…, docs/…; directories created)
//   GET  /out/<path>      -> <repo>/<path>  (a filtered run reads the manifest back to merge it)
//   POST /log             -> OUT_DIR/export.log (progress and errors from the page)
//   GET  /status          -> JSON summary, polled by the script
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const REPO = path.resolve(HERE, '..', '..');
const ROOT = process.env.DESIGN_ROOT || path.join(REPO, 'docs', 'design');
const OUT = process.env.OUT_DIR || REPO;
const LOG_DIR = process.env.LOG_DIR || HERE;
const PORT = +(process.env.PORT || 8765);
const VENDOR = path.join(HERE, 'node_modules');

const MIME = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.mjs': 'text/javascript; charset=utf-8',
  '.jsx': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8', '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml', '.png': 'image/png', '.jpg': 'image/jpeg', '.md': 'text/markdown; charset=utf-8',
  '.tsv': 'text/tab-separated-values; charset=utf-8', '.woff2': 'font/woff2', '.woff': 'font/woff', '.ttf': 'font/ttf',
};

fs.mkdirSync(OUT, { recursive: true });
const logFile = path.join(LOG_DIR, 'export.log');
try { fs.unlinkSync(logFile); } catch {}
const log = (s) => fs.appendFileSync(logFile, `${new Date().toISOString()} ${s}\n`);

const safe = (base, rel) => {
  const p = path.normalize(path.join(base, rel));
  if (!p.startsWith(base)) return null;
  return p;
};

const readBody = (req) => new Promise((resolve, reject) => {
  const chunks = [];
  req.on('data', (c) => chunks.push(c));
  req.on('end', () => resolve(Buffer.concat(chunks)));
  req.on('error', reject);
});

const serveFile = (res, file) => {
  fs.stat(file, (err, st) => {
    if (err || !st.isFile()) { res.writeHead(404); res.end('not found: ' + file); return; }
    res.writeHead(200, { 'content-type': MIME[path.extname(file).toLowerCase()] || 'application/octet-stream', 'content-length': st.size, 'cache-control': 'no-store' });
    fs.createReadStream(file).pipe(res);
  });
};

let written = 0, bytes = 0;
http.createServer(async (req, res) => {
  const url = new URL(req.url, 'http://localhost');
  const rel = decodeURIComponent(url.pathname);
  try {
    if (req.method === 'PUT' && rel.startsWith('/out/')) {
      const file = safe(OUT, rel.slice('/out/'.length));
      if (!file) { res.writeHead(400); res.end(); return; }
      const body = await readBody(req);
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, body);
      written++; bytes += body.length;
      res.writeHead(200); res.end('ok');
      return;
    }
    if (req.method === 'GET' && rel.startsWith('/out/')) {
      const file = safe(OUT, rel.slice('/out/'.length));
      if (!file) { res.writeHead(400); res.end(); return; }
      serveFile(res, file);
      return;
    }
    if (req.method === 'POST' && rel === '/log') {
      log((await readBody(req)).toString('utf8'));
      res.writeHead(200); res.end('ok');
      return;
    }
    if (rel === '/status') {
      let tail = '';
      try { tail = fs.readFileSync(logFile, 'utf8').trim().split('\n').slice(-5).join('\n'); } catch {}
      res.writeHead(200, { 'content-type': 'application/json' });
      res.end(JSON.stringify({ written, bytes, tail }, null, 2));
      return;
    }
    if (rel.startsWith('/vendor/')) {
      const file = safe(VENDOR, rel.slice('/vendor/'.length));
      if (!file) { res.writeHead(400); res.end(); return; }
      serveFile(res, file);
      return;
    }
    if (rel === '/export.html') { serveFile(res, path.join(HERE, 'export.html')); return; }
    if (rel === '/') {
      // The export page runs on load and overwrites game/assets, so the root
      // is an index rather than the page itself.
      res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' });
      res.end('<h3>Deep Field design export host</h3><ul><li><a href="/export.html">Export everything into the repo</a></li>'
        + '<li><a href="/export.html?only=lance">Export one tower — <code>?only=&lt;tower|item|stem&gt;</code>, comma-separated</a></li>'
        + '<li><a href="/export.html?only=hands">Export the hand poses only</a></li></ul>');
      return;
    }
    const file = safe(ROOT, rel);
    if (!file) { res.writeHead(400); res.end(); return; }
    serveFile(res, file);
  } catch (err) {
    log('server error: ' + err.stack);
    res.writeHead(500); res.end(String(err));
  }
}).listen(PORT, '127.0.0.1', () => {
  console.log(`design export host on http://127.0.0.1:${PORT}/  root=${ROOT}  out=${OUT}`);
});
