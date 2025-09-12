import threading
import time
import subprocess


class WiFiService:
    """Service to monitor WiFi connection status"""
    def __init__(self):
        self.running = False
        self.wifi_connected = False
        self.data_lock = threading.Lock()
    
    def check_wifi_status(self):
        """Check WiFi connection status"""
        try:
            result = subprocess.run(['hostname', '-I'], capture_output=True, text=True, timeout=1)
            return result.returncode == 0 and result.stdout.strip()
        except:
            return False
    
    def start(self):
        """Start the WiFi monitoring service"""
        self.running = True
        wifi_thread = threading.Thread(target=self._wifi_worker, daemon=True)
        wifi_thread.start()
        print("WiFi service started")
    
    def _wifi_worker(self):
        """Background thread that monitors WiFi status"""
        while self.running:
            try:
                connected = self.check_wifi_status()
                with self.data_lock:
                    self.wifi_connected = connected
            except Exception as e:
                print(f"WiFi service error: {e}")
            
            time.sleep(5)  # Check every 5 seconds
    
    def get_status(self):
        """Get current WiFi status (thread-safe)"""
        with self.data_lock:
            return self.wifi_connected
    
    def stop(self):
        """Stop the WiFi service"""
        self.running = False