import os
import sys
from urllib.parse import parse_qs


query = parse_qs(os.environ.get("QUERY_STRING", ""))
code_text = query.get("code", ["201"])[0]
code = int(code_text) if code_text.isdigit() else 201
if code < 200 or code > 599:
    code = 500

sys.stdout.write("Status: " + str(code) + " Custom CGI Status\r\n")
sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("status=" + str(code) + "\n")
