import sys


sys.stdout.write("Status: 302 Found\r\n")
sys.stdout.write("Location: /\r\n")
sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("redirecting\n")
