#!/usr/bin/python3
# POST /cgi-bin/echo.py  -> リクエストボディをそのまま返す
import os
import sys

length = int(os.environ.get("CONTENT_LENGTH") or 0)
body = sys.stdin.read(length) if length > 0 else ""

print("Content-Type: text/plain; charset=utf-8")
print()
print("=== echo ===")
print(body)
