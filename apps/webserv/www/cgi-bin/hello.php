<?php
header("Content-Type: text/plain; charset=utf-8");
header("X-CGI-Runtime: php");

echo "Hello from PHP-CGI\n";
echo "method=" . ($_SERVER["REQUEST_METHOD"] ?? "") . "\n";
echo "query=" . ($_SERVER["QUERY_STRING"] ?? "") . "\n";
?>
