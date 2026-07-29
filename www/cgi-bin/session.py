import os
import re
import secrets
import sys


cookie = os.environ.get("HTTP_COOKIE", "")
match = re.search(r"(?:^|;\s*)webserv_session=([a-f0-9]{32})(?:;|$)", cookie)
session_id = match.group(1) if match else secrets.token_hex(16)

sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
if match is None:
    sys.stdout.write(
        "Set-Cookie: webserv_session="
        + session_id
        + "; Path=/; HttpOnly; SameSite=Lax\r\n"
    )
sys.stdout.write("\r\n")
sys.stdout.write("session_id=" + session_id + "\n")
sys.stdout.write("existing=" + ("yes" if match else "no") + "\n")
