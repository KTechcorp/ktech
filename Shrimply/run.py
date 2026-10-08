"""Shrimply local development server.
Run: python run.py
Then open: http://127.0.0.1:8000
"""
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
import os, webbrowser

ROOT = Path(__file__).resolve().parent
os.chdir(ROOT)

class Handler(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        print(f"[Shrimply] {self.address_string()} - {fmt % args}")

server = ThreadingHTTPServer(("127.0.0.1", 8000), Handler)
print("\n🦐 Shrimply is running!")
print("Local: http://127.0.0.1:8000")
print("Press Ctrl+C to stop.\n")
webbrowser.open("http://127.0.0.1:8000")
try:
    server.serve_forever()
except KeyboardInterrupt:
    print("\nShrimply stopped.")
finally:
    server.server_close()
