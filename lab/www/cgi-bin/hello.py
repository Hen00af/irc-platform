#!/usr/bin/python3
# GET /cgi-bin/hello.py?name=seiya -> "Hello, seiya!"
import os
import urllib.parse

qs = os.environ.get("QUERY_STRING", "")
params = urllib.parse.parse_qs(qs)
name = params.get("name", ["World"])[0]

print("Content-Type: text/html; charset=utf-8")
print()
print(f"<h1>Hello, {name}!</h1>")
