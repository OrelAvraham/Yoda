#!/usr/bin/python3
import os
import socket
import logging

logging.basicConfig(level=logging.DEBUG, format="%(message)s")

def serve(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.bind(("", port))
        server_socket.listen(5)
        logging.info(f"Server listening on port {port}")

        while True:
            conn, addr = server_socket.accept()
            logging.info(f"Connection from {addr} ({conn.fileno()})")
            with conn:
                try:
                    while True:
                        data = conn.recv(1024)
                        if not data:
                            break
                        logging.info(f"Received: {data}")
                        conn.sendall(data)
                        logging.info(f"Sent: {data}")
                except Exception as e:
                    logging.error(f"Error with {addr}: {e}")
                finally:
                    logging.info(f"Connection with {addr} closed")


def main():
    print(f"Server PID: {os.getpid()}")

    port = int(os.getenv("PORT", 1111))
    serve(port)



if __name__ == "__main__":
    main()
