import os
import sys
import time
from urllib.parse import parse_qs


query = parse_qs(os.environ.get("QUERY_STRING", ""))
if "sleep" in query:
    time.sleep(float(query["sleep"][0]))
if query.get("mode") == ["error"]:
    raise RuntimeError("intentional CGI failure")
if query.get("mode") == ["malformed"]:
    sys.stdout.write("not a CGI response")
    sys.exit(0)

length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
body = sys.stdin.buffer.read(length).decode("utf-8", errors="replace")

sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("X-CGI: webserv\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("method=" + os.environ.get("REQUEST_METHOD", "") + "\n")
sys.stdout.write("query=" + os.environ.get("QUERY_STRING", "") + "\n")
sys.stdout.write("body=" + body + "\n")
sys.stdout.write("remote_addr=" + os.environ.get("REMOTE_ADDR", "") + "\n")
sys.stdout.write("request_uri=" + os.environ.get("REQUEST_URI", "") + "\n")
