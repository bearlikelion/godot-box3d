#!/usr/bin/env python3
from __future__ import annotations

import argparse
import functools
import http.server
import json
import socket
import threading
import time
from pathlib import Path


class SmokeHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self) -> None:
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

    def log_message(self, format: str, *args: object) -> None:
        return


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the exported Box3D Web smoke scene.")
    parser.add_argument("--dir", default="build/web-smoke")
    parser.add_argument("--browser-executable", default="")
    parser.add_argument("--screenshot", default="")
    parser.add_argument("--timeout", type=float, default=45.0)
    args = parser.parse_args()

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print(
            "Playwright is required: python3 -m pip install playwright && "
            "python3 -m playwright install chromium"
        )
        return 2

    root = Path(args.dir).resolve()
    if not (root / "index.html").is_file():
        print(f"Missing Web export: {root / 'index.html'}")
        return 1

    port = free_port()
    server = http.server.ThreadingHTTPServer(
        ("127.0.0.1", port),
        functools.partial(SmokeHandler, directory=str(root)),
    )
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()

    console_lines: list[str] = []
    errors: list[str] = []
    try:
        with sync_playwright() as playwright:
            launch_options: dict[str, object] = {"headless": True}
            if args.browser_executable:
                launch_options["executable_path"] = args.browser_executable
            browser = playwright.chromium.launch(**launch_options)
            page = browser.new_page(viewport={"width": 1280, "height": 720})

            def handle_console(message: object) -> None:
                text = message.text
                if message.type == "error" and (
                    text.startswith("ERROR:")
                    or "Aborted(" in text
                    or "Assertion failed" in text
                    or "RESULT: FAIL" in text
                ):
                    errors.append(f"console.error: {text}")
                else:
                    console_lines.append(text)

            page.on("console", handle_console)
            page.on("pageerror", lambda error: errors.append(f"pageerror: {error}"))
            response = page.goto(
                f"http://127.0.0.1:{port}/index.html",
                wait_until="domcontentloaded",
                timeout=int(args.timeout * 1000),
            )
            deadline = time.monotonic() + args.timeout
            while "RESULT: PASS" not in console_lines and time.monotonic() < deadline:
                page.wait_for_timeout(100)

            if args.screenshot:
                screenshot = Path(args.screenshot)
                screenshot.parent.mkdir(parents=True, exist_ok=True)
                page.screenshot(path=str(screenshot))
            browser.close()
    finally:
        server.shutdown()
        server.server_close()

    required = {
        "backend": any("Requested backend: Box3D Physics (Extension)" in line for line in console_lines),
        "extension": any("Extension class registered: true" in line for line in console_lines),
        "physics": "RESULT: PASS" in console_lines,
    }
    result = {
        "ok": bool(response and response.ok) and all(required.values()) and not errors,
        "required": required,
        "errors": errors,
        "console_tail": console_lines[-12:],
    }
    print(json.dumps(result, indent=2))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
