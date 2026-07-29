import os
import sys
from urllib.parse import parse_qs


length = int(os.environ.get("CONTENT_LENGTH", "0") or "0")
body = sys.stdin.buffer.read(length).decode("utf-8", errors="replace")
query = parse_qs(os.environ.get("QUERY_STRING", ""), keep_blank_values=True)

sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("method=" + os.environ.get("REQUEST_METHOD", "") + "\n")
sys.stdout.write("name=" + query.get("name", [""])[0] + "\n")
sys.stdout.write("body=" + body + "\n")
