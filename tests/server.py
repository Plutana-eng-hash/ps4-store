#!/usr/bin/env python3
"""Local test server: serves feed.json, a /redir -> /files/... 302 redirect, and Range-capable files."""
import http.server, os, re, sys, json
ROOT = sys.argv[1]; PORT = int(sys.argv[2])
class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def do_GET(self):
        p = self.path
        if p == "/feed.json":
            body = json.dumps([{"name": "Test Game", "title_id": "CUSA99999", "version": "01.00", "region": "USA",
                                "pkg": f"http://localhost:{PORT}/redir"}]).encode()
            return self.send(200, body, {"Content-Type": "application/json"})
        if p == "/redir":
            self.send_response(302); self.send_header("Location", "/files/Test%20Game.pkg"); self.send_header("Content-Length", "0"); self.end_headers(); return
        if p.startswith("/files/"):
            data = open(os.path.join(ROOT, "src.pkg"), "rb").read()
            rng = self.headers.get("Range")
            if rng:
                m = re.match(r"bytes=(\d+)-", rng); start = int(m.group(1))
                if start >= len(data): return self.send(416, b"", {})
                return self.send(206, data[start:], {"Content-Range": f"bytes {start}-{len(data)-1}/{len(data)}"})
            return self.send(200, data, {})
        self.send(404, b"nope", {})
    def send(self, code, body, hdrs):
        self.send_response(code)
        for k, v in hdrs.items(): self.send_header(k, v)
        self.send_header("Content-Length", str(len(body))); self.send_header("Connection", "close"); self.end_headers()
        try:
            if os.environ.get("SLOW") and len(body) > 1000000:      # throttle big bodies (~12 MB/s) for cancel/resume tests
                import time
                for i in range(0, len(body), 262144): self.wfile.write(body[i:i + 262144]); time.sleep(0.02)
            else: self.wfile.write(body)
        except Exception: pass
http.server.ThreadingHTTPServer(("127.0.0.1", PORT), H).serve_forever()
