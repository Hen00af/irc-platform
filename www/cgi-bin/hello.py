import os
import sys


sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("Hello from webserv CGI\n")
sys.stdout.write("method=" + os.environ.get("REQUEST_METHOD", "") + "\n")
