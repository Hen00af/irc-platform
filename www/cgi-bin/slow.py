import os
import sys
import time
from urllib.parse import parse_qs


query = parse_qs(os.environ.get("QUERY_STRING", ""))
seconds_text = query.get("seconds", ["5"])[0]
seconds = float(seconds_text)
seconds = max(0.0, min(seconds, 10.0))
time.sleep(seconds)

sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("slept=" + str(seconds) + "\n")
