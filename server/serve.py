#!/usr/bin/env python3
"""Tiny test server. Put list.txt and a files/ folder next to this script, then:
   python3 serve.py   ->  serves on port 8080 (matches config.h)"""
import http.server, socketserver
class H(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.0"
with socketserver.TCPServer(("", 8080), H) as s:
    print("Serving on :8080"); s.serve_forever()
