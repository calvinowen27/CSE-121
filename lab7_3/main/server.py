from http.server import BaseHTTPRequestHandler, HTTPServer
import logging
import requests

class MyRequestHandler(BaseHTTPRequestHandler):
	def do_GET(self):
		# Handle GET requests
		self.send_response(200)
		self.send_header('Content-type', 'text/html')
		self.end_headers()
		self.wfile.write(b"Hello! You made a GET request.")

	def do_POST(self):
		# Handle POST requests
		content_length = int(self.headers['Content-Length'])
		post_data = self.rfile.read(content_length)
		# logging.info(f"Temperature and Humidity: {post_data.decode('utf-8')}\n")
		zipcode = get_ip_zipcode()
		temp = get_wttr(zipcode)
		print(f'Final Result: Temperature in zipcode {zipcode} is {temp}C according to wttr.in. Sensor says {post_data.decode("utf-8")}.\n')

		# Send a response back to the client
		self.send_response(200)
		self.send_header('Content-type', 'text/html')
		self.end_headers()
		self.wfile.write(f'postal {zipcode}'.encode('utf-8'))

def run(server_class=HTTPServer, handler_class=MyRequestHandler, port=1234):
	logging.basicConfig(level=logging. INFO)
	server_address = ('', port)
	httpd = server_class(server_address, handler_class)
	logging.info(f"Starting server on port {port} ...")
	httpd.serve_forever()

def get_wttr(zipcode):
	r = requests.get(f'http://wttr.in/{zipcode}?format=%t')
	return int(r.content.decode('utf-8')[1:-2])

def get_ip_zipcode():
	r = requests.get('http://ipinfo.io')
	return int(r.json()['postal'])

if __name__ == '__main__':
	run()