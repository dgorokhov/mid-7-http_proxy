import http.server
import socketserver
import http.client

PORT = 9000
PROXY_PORT = 8080

class HTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/empty':
            self.send_response(200)
            self.send_header('Content-Length', '0')
            self.end_headers()
        elif self.path == '/no-length':
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"Data without content length, closing connection.")
            # Явно закрываем соединение на стороне мишени, чтобы прокси поймал EOF
            self.close_connection = True 
        else:
            self.send_response(200)
            self.send_header('Content-Length', '13')
            self.end_headers()
            self.wfile.write(b"Hello, World!")

def run_tests():
    # Подключаемся НАПРЯМУЮ к нашему C++ прокси
    def make_proxy_request(path):
        conn = http.client.HTTPConnection("127.0.0.1", PROXY_PORT, timeout=5)
        # Формируем валидный HTTP запрос, который прокси сможет распарсить.
        # В ТЗ прокси ориентируется на заголовок Host.
        headers = {
            "Host": f"127.0.0.1:{PORT}",
            "Connection": "close"
        }
        # Шлем запрос прокси-серверу, запрашивая полный URL мишени
        conn.request("GET", f"http://127.0.0.1:{PORT}{path}", headers=headers)
        response = conn.getresponse()
        body = response.read().decode()
        conn.close()
        return response.status, body

    try:
        # 1. Тест обычного запроса
        print("\n--- Test 1: Standard Request ---")
        status, body = make_proxy_request("/")
        print("Status:", status)
        print("Body:", body)
        
        # 2. Тест пустого тела
        print("\n--- Test 2: Empty Body ---")
        status, body = make_proxy_request("/empty")
        print("Status:", status)
        print("Body:", body)

        # 3. Тест без Content-Length (чтение до EOF)
        print("\n--- Test 3: No Content-Length ---")
        status, body = make_proxy_request("/no-length")
        print("Status:", status)
        print("Body:", body)
        
    except Exception as e:
        print("Proxy test failed with error:", e)

# Запускаем локальный веб-сервер
if __name__ == "__main__":
    # socketserver.TCPServer.allow_reuse_address = True позволит перезапускать скрипт без залипания портов
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), HTTPRequestHandler) as httpd:
        print(f"Target test server running on port {PORT}")
        print(f"Sending requests via C++ Proxy on port {PROXY_PORT}...")
        
        # Выполняем тесты
        run_tests()
