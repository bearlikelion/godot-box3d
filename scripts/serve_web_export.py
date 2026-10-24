#!/usr/bin/env python3
"""Serve a Godot Web export locally with extension-compatible headers."""

from __future__ import annotations

import argparse
import functools
import http.server
from pathlib import Path


class GodotWebHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Directory containing index.html.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8060)
    args = parser.parse_args()

    root = args.directory.resolve()
    if not (root / "index.html").is_file():
        parser.error(f"Web export is missing: {root / 'index.html'}")

    handler = functools.partial(GodotWebHandler, directory=str(root))
    try:
        server = http.server.ThreadingHTTPServer((args.host, args.port), handler)
    except OSError as exc:
        parser.error(f"could not listen on {args.host}:{args.port}: {exc}")
    bound_port = int(server.server_address[1])
    print(f"Serving {root} at http://{args.host}:{bound_port}/", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
