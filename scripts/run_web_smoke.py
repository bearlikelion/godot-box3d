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


def required_checks(lines: list[str]) -> dict[str, bool]:
    return {
        "backend": "Requested backend: Box3D Physics" in lines,
        "active_backend": "Active backend: Box3DPhysicsServer3D" in lines,
        "extension": "Extension class registered: true" in lines,
        "area": "Area event received: true" in lines,
        "hinge": any(line.startswith("Door rotation delta: ") for line in lines),
        "physics": "RESULT: PASS" in lines,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the exported Box3D Web smoke scene.")
    parser.add_argument("--dir", default="build/web-smoke")
    parser.add_argument("--browser-executable", default="")
    parser.add_argument("--browser", choices=("chromium", "firefox", "webkit"), default="chromium")
    parser.add_argument("--screenshot", default="")
    parser.add_argument("--timeout", type=float, default=45.0)
    parser.add_argument("--reloads", type=int, default=0, help="Additional page loads in the same browser context.")
    parser.add_argument("--observe-seconds", type=float, default=0.0, help="Watch for errors after the final PASS; not a performance or memory qualification.")
    args = parser.parse_args()
    if args.reloads < 0 or args.observe_seconds < 0 or args.timeout <= 0:
        parser.error("reloads/observe-seconds must be nonnegative and timeout must be positive")

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
    loads: list[dict[str, object]] = []
    try:
        with sync_playwright() as playwright:
            launch_options: dict[str, object] = {"headless": True}
            if args.browser_executable:
                launch_options["executable_path"] = args.browser_executable
            browser = getattr(playwright, args.browser).launch(**launch_options)
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
            for attempt in range(args.reloads + 1):
                console_lines.clear()
                errors.clear()
                started = time.monotonic()
                response = page.goto(
                    f"http://127.0.0.1:{port}/index.html",
                    wait_until="domcontentloaded",
                    timeout=int(args.timeout * 1000),
                )
                deadline = started + args.timeout
                while "RESULT: PASS" not in console_lines and not errors and time.monotonic() < deadline:
                    page.wait_for_timeout(100)
                passed = bool(response and response.ok) and all(required_checks(console_lines).values()) and not errors
                loads.append({"load": attempt, "seconds_to_physics_pass": round(time.monotonic() - started, 3), "ok": passed})
                if not passed:
                    break
            if loads[-1]["ok"] and args.observe_seconds:
                page.wait_for_timeout(args.observe_seconds * 1000)

            if args.screenshot:
                screenshot = Path(args.screenshot)
                screenshot.parent.mkdir(parents=True, exist_ok=True)
                page.screenshot(path=str(screenshot))
            browser.close()
    finally:
        server.shutdown()
        server.server_close()

    required = required_checks(console_lines)
    result = {
        "browser": args.browser,
        "ok": len(loads) == args.reloads + 1 and all(load["ok"] for load in loads) and not errors,
        "required": required,
        "loads": loads,
        "post_pass_observation_seconds": args.observe_seconds,
        "errors": errors,
        "console_tail": console_lines[-20:],
    }
    print(json.dumps(result, indent=2))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
