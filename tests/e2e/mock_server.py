#!/usr/bin/env python3
"""
Mock HTTP Server for Streamripper E2E Tests

This server simulates an Icecast/Shoutcast streaming server for testing purposes.
It serves fixture stream data with optional ICY metadata injection.

Usage:
    python mock_server.py [port] [stream_file]

    port: Port to listen on (default: 8765)
    stream_file: Path to fixture stream data (default: ../fixtures/streams/test_stream.dat)

Features:
    - ICY 200 OK protocol response
    - ICY metadata injection at configurable intervals
    - Supports MP3 and OGG content types
    - Graceful shutdown via /shutdown endpoint
    - Connection drop simulation for reconnect testing
"""

import argparse
import os
import signal
import socket
import sys
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from io import BytesIO


# Default configuration
DEFAULT_PORT = 8765
DEFAULT_META_INTERVAL = 8192
DEFAULT_BITRATE = 128

# Global server instance for shutdown handling
_server_instance = None
_shutdown_event = threading.Event()


class MockStreamHandler(BaseHTTPRequestHandler):
    """HTTP handler that simulates Icecast/Shoutcast streaming server."""

    # Class-level configuration (set by server setup)
    stream_file = None
    meta_interval = DEFAULT_META_INTERVAL
    station_name = "Test Radio Station"
    station_url = "http://localhost:8765"
    bitrate = DEFAULT_BITRATE
    content_type = "audio/mpeg"
    drop_after_bytes = 0  # 0 = don't drop, >0 = drop after N bytes
    metadata_sequence = []  # List of metadata strings to cycle through

    def log_message(self, format, *args):
        """Suppress default logging unless DEBUG env var is set."""
        if os.environ.get('DEBUG'):
            super().log_message(format, *args)

    def do_GET(self):
        """Handle GET requests."""
        if self.path == '/shutdown':
            self._handle_shutdown()
        elif self.path == '/status':
            self._handle_status()
        elif self.path == '/config':
            self._handle_config()
        elif self.path.startswith('/stream'):
            self._handle_stream()
        elif self.path == '/drop':
            # Special endpoint that drops connection after sending some data
            self._handle_drop_stream()
        else:
            self._handle_stream()

    def _handle_shutdown(self):
        """Handle shutdown request."""
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b'OK\n')
        _shutdown_event.set()

    def _handle_status(self):
        """Handle status check request."""
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.end_headers()
        self.wfile.write(b'OK\n')

    def _handle_config(self):
        """Handle configuration request - returns current settings as JSON."""
        import json
        config = {
            'station_name': self.station_name,
            'bitrate': self.bitrate,
            'meta_interval': self.meta_interval,
            'content_type': self.content_type,
            'drop_after_bytes': self.drop_after_bytes
        }
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(config).encode('utf-8'))

    def _handle_stream(self):
        """Handle stream request - sends ICY response with stream data."""
        # Check if client wants ICY metadata
        icy_metadata = self.headers.get('Icy-MetaData', '0') == '1'

        # Send ICY response headers
        self._send_icy_headers(icy_metadata)

        # Stream the data
        self._stream_data(icy_metadata, drop_connection=False)

    def _handle_drop_stream(self):
        """Handle stream that intentionally drops connection."""
        icy_metadata = self.headers.get('Icy-MetaData', '0') == '1'
        self._send_icy_headers(icy_metadata)
        self._stream_data(icy_metadata, drop_connection=True)

    def _send_icy_headers(self, icy_metadata):
        """Send ICY protocol headers."""
        # Use raw socket write to send ICY response (not HTTP/1.x)
        response = "ICY 200 OK\r\n"
        response += f"icy-name:{self.station_name}\r\n"
        response += f"icy-url:{self.station_url}\r\n"
        response += f"icy-br:{self.bitrate}\r\n"
        response += "icy-genre:Test\r\n"
        response += f"Content-Type:{self.content_type}\r\n"

        if icy_metadata:
            response += f"icy-metaint:{self.meta_interval}\r\n"

        response += "\r\n"

        self.wfile.write(response.encode('utf-8'))

    def _stream_data(self, icy_metadata, drop_connection=False):
        """Stream audio data with optional ICY metadata."""
        # Load stream data
        stream_data = self._load_stream_data()
        if not stream_data:
            return

        bytes_sent = 0
        bytes_since_meta = 0
        metadata_index = 0
        drop_bytes = self.drop_after_bytes if drop_connection else 0

        try:
            data_index = 0
            while not _shutdown_event.is_set():
                # Calculate how much data to send
                if icy_metadata:
                    chunk_size = min(self.meta_interval - bytes_since_meta,
                                    len(stream_data) - data_index)
                else:
                    chunk_size = min(4096, len(stream_data) - data_index)

                if chunk_size <= 0:
                    # Loop the stream data
                    data_index = 0
                    continue

                # Check if we should drop the connection
                if drop_bytes > 0 and bytes_sent >= drop_bytes:
                    return  # Abruptly close connection

                # Send audio data chunk
                chunk = stream_data[data_index:data_index + chunk_size]
                self.wfile.write(chunk)
                self.wfile.flush()

                data_index += chunk_size
                bytes_sent += chunk_size
                bytes_since_meta += chunk_size

                # Insert metadata if needed
                if icy_metadata and bytes_since_meta >= self.meta_interval:
                    self._send_metadata(metadata_index)
                    if self.metadata_sequence:
                        metadata_index = (metadata_index + 1) % len(self.metadata_sequence)
                    bytes_since_meta = 0

                # Small delay to simulate realistic streaming
                time.sleep(0.01)

        except (BrokenPipeError, ConnectionResetError):
            pass  # Client disconnected

    def _load_stream_data(self):
        """Load stream data from fixture file or generate dummy data."""
        if self.stream_file and os.path.exists(self.stream_file):
            with open(self.stream_file, 'rb') as f:
                return f.read()
        else:
            # Generate dummy MP3-like frame data
            # Simple pattern that looks like MP3 frame headers
            return self._generate_dummy_mp3_data(64 * 1024)  # 64KB of dummy data

    def _generate_dummy_mp3_data(self, size):
        """Generate dummy data that resembles MP3 frames."""
        # MP3 frame sync word: 0xFF 0xFB (MPEG1 Layer3)
        # Frame size for 128kbps @ 44100Hz = 417 bytes
        frame_size = 417
        frame_header = bytes([0xFF, 0xFB, 0x90, 0x00])  # MP3 frame header

        data = BytesIO()
        while data.tell() < size:
            # Write frame header
            data.write(frame_header)
            # Write frame padding (zeros would work, but use pattern for visibility)
            padding_size = frame_size - len(frame_header)
            data.write(bytes([0x00] * padding_size))

        return data.getvalue()[:size]

    def _send_metadata(self, index=0):
        """Send ICY metadata block."""
        if self.metadata_sequence and index < len(self.metadata_sequence):
            title = self.metadata_sequence[index]
        else:
            title = "Test Artist - Test Song"

        metadata = f"StreamTitle='{title}';"

        # Metadata length is encoded as (length / 16), padded to 16-byte boundary
        meta_bytes = metadata.encode('utf-8')
        padded_len = ((len(meta_bytes) + 15) // 16) * 16
        length_byte = padded_len // 16

        # Send length byte + metadata + padding
        self.wfile.write(bytes([length_byte]))
        self.wfile.write(meta_bytes)
        self.wfile.write(bytes([0] * (padded_len - len(meta_bytes))))
        self.wfile.flush()


class MockStreamServer(HTTPServer):
    """HTTP server with configurable stream parameters."""

    allow_reuse_address = True

    def __init__(self, port, stream_file=None, **kwargs):
        """Initialize the mock server.

        Args:
            port: Port to listen on
            stream_file: Path to fixture stream data file
            **kwargs: Additional configuration options:
                - meta_interval: ICY metadata interval (default: 8192)
                - station_name: Station name for ICY headers
                - bitrate: Reported bitrate
                - content_type: MIME type of stream
                - drop_after_bytes: Drop connection after N bytes (0 = never)
                - metadata_sequence: List of metadata strings to cycle
        """
        super().__init__(('0.0.0.0', port), MockStreamHandler)

        # Configure handler class
        MockStreamHandler.stream_file = stream_file
        MockStreamHandler.meta_interval = kwargs.get('meta_interval', DEFAULT_META_INTERVAL)
        MockStreamHandler.station_name = kwargs.get('station_name', 'Test Radio Station')
        MockStreamHandler.bitrate = kwargs.get('bitrate', DEFAULT_BITRATE)
        MockStreamHandler.content_type = kwargs.get('content_type', 'audio/mpeg')
        MockStreamHandler.drop_after_bytes = kwargs.get('drop_after_bytes', 0)
        MockStreamHandler.metadata_sequence = kwargs.get('metadata_sequence', [])

    def serve_until_shutdown(self):
        """Serve requests until shutdown is requested."""
        while not _shutdown_event.is_set():
            self.handle_request()


def start_server(port=DEFAULT_PORT, stream_file=None, background=False, **kwargs):
    """Start the mock streaming server.

    Args:
        port: Port to listen on
        stream_file: Path to fixture stream data
        background: If True, run in a background thread
        **kwargs: Additional configuration options

    Returns:
        Server instance if background=True, else blocks until shutdown
    """
    global _server_instance

    _shutdown_event.clear()
    _server_instance = MockStreamServer(port, stream_file, **kwargs)

    if background:
        thread = threading.Thread(target=_server_instance.serve_until_shutdown)
        thread.daemon = True
        thread.start()
        return _server_instance
    else:
        print(f"Mock streaming server running on port {port}")
        print("Press Ctrl+C or GET /shutdown to stop")
        try:
            _server_instance.serve_until_shutdown()
        except KeyboardInterrupt:
            pass
        finally:
            _server_instance.server_close()


def stop_server():
    """Stop the mock server."""
    _shutdown_event.set()
    if _server_instance:
        _server_instance.server_close()


def wait_for_server(host='localhost', port=DEFAULT_PORT, timeout=5):
    """Wait for server to be ready.

    Args:
        host: Server host
        port: Server port
        timeout: Max time to wait in seconds

    Returns:
        True if server is ready, False if timeout
    """
    import urllib.request
    import urllib.error

    start = time.time()
    while time.time() - start < timeout:
        try:
            urllib.request.urlopen(f'http://{host}:{port}/status', timeout=1)
            return True
        except (urllib.error.URLError, socket.timeout):
            time.sleep(0.1)
    return False


def main():
    """Main entry point for command-line usage."""
    parser = argparse.ArgumentParser(description='Mock Icecast/Shoutcast server for testing')
    parser.add_argument('--port', '-p', type=int, default=DEFAULT_PORT,
                       help=f'Port to listen on (default: {DEFAULT_PORT})')
    parser.add_argument('--stream', '-s', type=str, default=None,
                       help='Path to stream fixture file')
    parser.add_argument('--meta-interval', '-m', type=int, default=DEFAULT_META_INTERVAL,
                       help=f'ICY metadata interval (default: {DEFAULT_META_INTERVAL})')
    parser.add_argument('--station-name', type=str, default='Test Radio Station',
                       help='Station name for ICY headers')
    parser.add_argument('--bitrate', '-b', type=int, default=DEFAULT_BITRATE,
                       help=f'Reported bitrate (default: {DEFAULT_BITRATE})')
    parser.add_argument('--content-type', '-c', type=str, default='audio/mpeg',
                       help='MIME content type (default: audio/mpeg)')
    parser.add_argument('--drop-after', type=int, default=0,
                       help='Drop connection after N bytes (for reconnect testing)')
    parser.add_argument('--metadata', type=str, nargs='*', default=[],
                       help='Metadata strings to cycle through')

    args = parser.parse_args()

    start_server(
        port=args.port,
        stream_file=args.stream,
        meta_interval=args.meta_interval,
        station_name=args.station_name,
        bitrate=args.bitrate,
        content_type=args.content_type,
        drop_after_bytes=args.drop_after,
        metadata_sequence=args.metadata
    )


if __name__ == '__main__':
    main()
