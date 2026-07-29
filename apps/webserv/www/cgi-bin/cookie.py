import sys


sys.stdout.write("Content-Type: text/plain; charset=utf-8\r\n")
sys.stdout.write("Set-Cookie: webserv=cgi; Path=/; HttpOnly; SameSite=Lax\r\n")
sys.stdout.write("\r\n")
sys.stdout.write("cookie set\n")
