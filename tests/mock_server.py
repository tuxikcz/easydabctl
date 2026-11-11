#!/usr/bin/env python3
from http.server import BaseHTTPRequestHandler, HTTPServer
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def do_GET(self):
        status=bytes.fromhex('80 00 40 00 02 00 00 00 00 04 00 17 b7 c0')
        dac=bytes.fromhex('8e 64 7a 00 00 55 fa e1 48 00 01 00 00 80')
        config=bytearray([255]*8000)
        config[22:28]=bytes([10,26,76,1,0x22,0xb8]);config[28]=4;config[38]=1
        if self.path=='/s_000000': payload=status
        elif self.path=='/s_000e00': payload=dac
        elif self.path=='/b_070000':payload=bytes(config)
        elif self.path in ['/s_025000','/s_020000','/s_010000','/s_010400','/s_010200','/r_000000'] or self.path.startswith('/s_000e'): payload=status
        else:self.send_error(404);return
        self.send_response(200);self.send_header('Content-Length',str(len(payload)));self.end_headers();self.wfile.write(payload)
HTTPServer(('127.0.0.1',18765),Handler).serve_forever()
