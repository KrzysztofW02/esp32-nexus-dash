"""
ESP32 Dashboard Controller
Companion script for ESP32 Nexus Dash - manages PC monitoring, Cloudflare proxy, and camera streaming.
"""

import multiprocessing
import customtkinter as ctk
import threading
import sys
import os
import time
from datetime import datetime, timedelta, timezone
from enum import Enum

from PIL import Image
import pystray
from pystray import MenuItem as item

import psutil
import requests
import cv2
import subprocess
import re
import html
import unicodedata
from flask import Flask, Response
import logging

logging.getLogger('werkzeug').setLevel(logging.ERROR)
os.environ['WERKZEUG_RUN_MAIN'] = 'true'

try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass


ESP32_IP = os.getenv('ESP32_IP', '192.168.1.47')

# Camera settings
CAMERA_IP = os.getenv('CAMERA_IP', '192.168.1.13')
CAMERA_USER = os.getenv('CAMERA_USER', 'admin')
CAMERA_PASS = os.getenv('CAMERA_PASS', '')
RTSP_URL = f"rtsp://{CAMERA_USER}:{CAMERA_PASS}@{CAMERA_IP}:554/live/ch1"

# Cloudflare API
CF_API_TOKEN = os.getenv('CF_API_TOKEN', '')
CF_ZONE_ID = os.getenv('CF_ZONE_ID', '')
CF_GRAPHQL_URL = 'https://api.cloudflare.com/client/v4/graphql'

# YouTube API
YT_API_KEY = os.getenv('YOUTUBE_API_KEY', '')
YT_CHANNEL_ID = os.getenv('YOUTUBE_CHANNEL_ID', '')
YT_API_BASE = 'https://www.googleapis.com/youtube/v3'


# --- GUI SETTINGS ---
ctk.set_appearance_mode("dark")
ctk.set_default_color_theme("blue")

def get_resource_path(relative_path):
    try:
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(base_path, relative_path)

SCRIPT_DIR = get_resource_path("")


class ServiceState(Enum):
    STOPPED = "OFFLINE"
    STARTING = "STARTING..."
    RUNNING = "RUNNING"
    STOPPING = "STOPPING..."
    ERROR = "ERROR"


class BaseService:
    """Base class for all services with thread-safe management"""
    
    def __init__(self, log_callback=None):
        self._state = ServiceState.STOPPED
        self._thread = None
        self._stop_event = threading.Event()
        self._lock = threading.Lock()
        self._log_callback = log_callback
    
    def log(self, message):
        if self._log_callback:
            try:
                self._log_callback(message)
            except:
                pass
    
    @property
    def state(self):
        with self._lock:
            return self._state
    
    @state.setter
    def state(self, value):
        with self._lock:
            self._state = value
    
    def is_running(self):
        return self.state in [ServiceState.RUNNING, ServiceState.STARTING]
    
    def can_start(self):
        return self.state == ServiceState.STOPPED or self.state == ServiceState.ERROR
    
    def can_stop(self):
        return self.state == ServiceState.RUNNING
    
    def start(self):
        with self._lock:
            if self._state not in [ServiceState.STOPPED, ServiceState.ERROR]:
                return False
            self._state = ServiceState.STARTING
            self._stop_event.clear()
        
        self._thread = threading.Thread(target=self._run_wrapper, daemon=True)
        self._thread.start()
        return True
    
    def stop(self):
        with self._lock:
            if self._state not in [ServiceState.RUNNING, ServiceState.STARTING]:
                return False
            self._state = ServiceState.STOPPING
        
        self._stop_event.set()
        
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=3.0)
        
        self.state = ServiceState.STOPPED
        return True
    
    def _run_wrapper(self):
        try:
            self.state = ServiceState.RUNNING
            self.log("Started")
            self._run()
        except Exception as e:
            self.log(f"Error: {str(e)[:40]}")
            self.state = ServiceState.ERROR
        finally:
            if self.state != ServiceState.ERROR:
                self.state = ServiceState.STOPPED
            self.log("Stopped")
    
    def _run(self):
        raise NotImplementedError
    
    def _should_stop(self):
        return self._stop_event.is_set()
    
    def _sleep(self, seconds):
        self._stop_event.wait(timeout=seconds)
        return self._should_stop()


# --- PC MONITOR SERVICE ---
class PCMonitorService(BaseService):
    """Collects PC stats (CPU, RAM, Network) and sends to ESP32"""
    
    def __init__(self, log_callback=None):
        super().__init__(log_callback)
        self.esp_url = f"http://{ESP32_IP}/update_pc"
        self.wmi_available = False
        self._init_wmi()
    
    def _init_wmi(self):
        """Try to initialize WMI connection to OpenHardwareMonitor"""
        try:
            import wmi
            self.wmi = wmi.WMI(namespace="root\\OpenHardwareMonitor")
            # Test if OHM is running
            sensors = self.wmi.Sensor()
            if sensors:
                self.wmi_available = True
        except:
            self.wmi_available = False
    
    def get_ohm_value(self, sensor_type, name_contains):
        """Get sensor value from OpenHardwareMonitor"""
        if not self.wmi_available:
            return 0
        try:
            for sensor in self.wmi.Sensor():
                if sensor.SensorType == sensor_type:
                    if name_contains.lower() in sensor.Name.lower():
                        return int(sensor.Value)
        except:
            pass
        return 0
    
    def get_cpu_temp(self):
        """Get CPU temperature (if available in OHM)"""
        return self.get_ohm_value('Temperature', 'CPU') or \
               self.get_ohm_value('Temperature', 'Core') or \
               self.get_ohm_value('Temperature', 'Package')
    
    def get_gpu_stats(self):
        """Get GPU load and temperature"""
        gpu_load = self.get_ohm_value('Load', 'GPU Core')
        gpu_temp = self.get_ohm_value('Temperature', 'GPU')
        return gpu_load, gpu_temp
    
    def get_ping(self, host="8.8.8.8"):
        """Get ping to Google DNS in ms"""
        try:
            if os.name == 'nt':
                result = subprocess.run(
                    ["ping", "-n", "1", "-w", "1000", host],
                    capture_output=True, text=True, timeout=3,
                    creationflags=subprocess.CREATE_NO_WINDOW
                )
            else:
                result = subprocess.run(
                    ["ping", "-c", "1", "-W", "1", host],
                    capture_output=True, text=True, timeout=3
                )
            
            if result.returncode == 0:
                match = re.search(r'(?:time|czas)[=<](\d+)', result.stdout, re.IGNORECASE)
                if match:
                    return int(match.group(1))
        except:
            pass
        return 0
    
    def get_stats(self):
        # CPU
        cpu_usage = int(psutil.cpu_percent(interval=0.5))
        cpu_temp = self.get_cpu_temp()
        
        # GPU
        gpu_load, gpu_temp = self.get_gpu_stats()
        
        # RAM
        ram = psutil.virtual_memory()
        ram_used = round(ram.used / (1024**3), 1)
        ram_total = round(ram.total / (1024**3))
        
        # Network speed (measure over 1 second)
        net_old = psutil.net_io_counters()
        if self._sleep(1):
            return None
        net_new = psutil.net_io_counters()
        
        sent_mb = (net_new.bytes_sent - net_old.bytes_sent) * 8 / 1024 / 1024
        recv_mb = (net_new.bytes_recv - net_old.bytes_recv) * 8 / 1024 / 1024
        
        # Ping
        ping = self.get_ping()
        
        return {
            "cpu_load": cpu_usage,
            "cpu_temp": cpu_temp,
            "gpu_load": gpu_load,
            "gpu_temp": gpu_temp,
            "ram_used": int(ram_used),
            "ram_total": int(ram_total),
            "net_up": round(sent_mb, 1),
            "net_down": round(recv_mb, 1),
            "ping": ping
        }
    
    def _run(self):
        while not self._should_stop():
            try:
                data = self.get_stats()
                if data is None:
                    break
                self.log(f"CPU: {data['cpu_load']}% | RAM: {data['ram_used']}GB")
                requests.post(self.esp_url, json=data, timeout=2)
            except requests.exceptions.RequestException:
                self.log("ESP32 unreachable")
            except Exception as e:
                self.log(f"Error: {str(e)[:30]}")
            
            if self._sleep(2):
                break


# --- CLOUDFLARE PROXY SERVICE ---
COUNTRY_CODES = {
    'United States': 'US', 'China': 'CN', 'Russia': 'RU', 'Germany': 'DE', 'Poland': 'PL',
    'Brazil': 'BR', 'France': 'FR', 'United Kingdom': 'GB', 'Netherlands': 'NL',
    'Ukraine': 'UA', 'Vietnam': 'VN', 'Indonesia': 'ID', 'India': 'IN', 'Japan': 'JP',
    'South Korea': 'KR', 'Thailand': 'TH', 'Bangladesh': 'BD', 'Pakistan': 'PK',
    'Philippines': 'PH', 'Singapore': 'SG', 'Turkey': 'TR', 'Iran': 'IR',
    'Australia': 'AU', 'Canada': 'CA', 'Mexico': 'MX', 'Italy': 'IT', 'Spain': 'ES',
    'Saudi Arabia': 'SA', 'Egypt': 'EG', 'Morocco': 'MA', 'Nigeria': 'NG'
}

class CloudflareProxyService(BaseService):
    """Fetches Cloudflare security analytics and sends to ESP32"""
    
    def __init__(self, log_callback=None):
        super().__init__(log_callback)
        self.esp_url = f"http://{ESP32_IP}/update_cloudflare"
    
    def get_code(self, name):
        return COUNTRY_CODES.get(name, name[:2].upper()) if name else 'XX'
    
    def get_type(self, source):
        s = source.lower() if source else ''
        if s in ['bic', 'hot', 'managed_challenge', 'bot_fight_mode', 'super_bot_fight_mode', 'linkmaze']: 
            return 'Bot'
        if s in ['l7ddos', 'http_ddos']: 
            return 'DDoS'
        if s == 'ratelimit': 
            return 'RateLim'
        return 'WAF'
    
    def get_http_totals(self):
        now = datetime.now(timezone.utc)
        past = now - timedelta(hours=24)
        headers = {'Authorization': f'Bearer {CF_API_TOKEN}', 'Content-Type': 'application/json'}
        query = """
        query GetHTTP($zoneTag: String!, $dateStart: String!, $dateEnd: String!) {
          viewer {
            zones(filter: {zoneTag: $zoneTag}) {
              http_summary: httpRequests1dGroups(limit: 1, filter: {date_geq: $dateStart, date_leq: $dateEnd}) {
                sum { requests cachedRequests }
              }
            }
          }
        }
        """
        variables = {'zoneTag': CF_ZONE_ID, 'dateStart': past.strftime('%Y-%m-%d'), 'dateEnd': now.strftime('%Y-%m-%d')}
        try: 
            return requests.post(CF_GRAPHQL_URL, headers=headers, json={'query': query, 'variables': variables}, timeout=30).json()
        except: 
            return None
    
    def get_raw_events(self):
        now = datetime.now(timezone.utc)
        past = now - timedelta(hours=24)
        headers = {'Authorization': f'Bearer {CF_API_TOKEN}', 'Content-Type': 'application/json'}
        query = """
        query GetEvents($zoneTag: String!, $start: String!, $end: String!) {
          viewer {
            zones(filter: {zoneTag: $zoneTag}) {
              feed: firewallEventsAdaptive(
                limit: 500, 
                filter: {datetime_geq: $start, datetime_leq: $end}, 
                orderBy: [datetime_DESC]
              ) {
                action source clientCountryName clientIP datetime
              }
            }
          }
        }
        """
        variables = {'zoneTag': CF_ZONE_ID, 'start': past.isoformat(), 'end': now.isoformat()}
        try: 
            return requests.post(CF_GRAPHQL_URL, headers=headers, json={'query': query, 'variables': variables}, timeout=30).json()
        except: 
            return None
    
    def process_data(self, http_data, events_data):
        final = {
            'total_requests': 0, 'blocked': 0, 'challenged': 0, 'threats': 0, 'cache_hit': 0, 
            'waf': 0, 'ddos': 0, 'bot': 0, 'rate_limit': 0, 'firewall': 0, 
            'top_countries': [], 'events': []
        }
        
        if http_data and 'data' in http_data:
            try:
                zone = http_data['data']['viewer']['zones'][0]
                if zone.get('http_summary'):
                    sums = zone['http_summary'][0].get('sum', {})
                    final['total_requests'] = sums.get('requests', 0)
                    cached = sums.get('cachedRequests', 0)
                    if final['total_requests'] > 0:
                        final['cache_hit'] = int((cached / final['total_requests']) * 100)
            except: 
                pass

        if events_data and 'data' in events_data:
            try:
                feed = events_data['data']['viewer']['zones'][0].get('feed', [])
                seen_ips = set()
                country_stats = {}
                
                for ev in feed:
                    act = ev.get('action', 'unknown')
                    src = ev.get('source', 'unknown')
                    t = self.get_type(src)
                    
                    if t == 'Bot': final['bot'] += 1
                    elif t == 'DDoS': final['ddos'] += 1
                    elif t == 'RateLim': final['rate_limit'] += 1
                    else: 
                        final['waf'] += 1
                        t = 'WAF'
                    
                    if act not in ['log', 'skip', 'allow', 'bypass']:
                        final['blocked'] += 1
                    
                    if act in ['challenge', 'managed_challenge', 'jschallenge']: 
                        final['challenged'] += 1
                    
                    c_name = ev.get('clientCountryName')
                    if c_name:
                        code = self.get_code(c_name)
                        country_stats[code] = country_stats.get(code, 0) + 1

                    ip = ev.get('clientIP', 'unknown')
                    if len(final['events']) < 10:
                        if ip == 'unknown' or ip not in seen_ips:
                            if ip != 'unknown': seen_ips.add(ip)
                            
                            try:
                                dt = datetime.fromisoformat(ev['datetime'].replace('Z', '+00:00'))
                                t_str = dt.strftime('%H:%M:%S')
                            except: 
                                t_str = '--:--'
                            
                            display_action = act.capitalize()
                            if act == 'link_maze_injected': display_action = 'maze'
                            elif act == 'managed_challenge': display_action = 'Managed'
                            elif act == 'jschallenge': display_action = 'JS Chall'
                            elif act == 'challenge': display_action = 'Captcha'
                            elif act == 'block': display_action = 'Block'
                            
                            final['events'].append({
                                'type': t,
                                'country': self.get_code(c_name),
                                'action': display_action[:9],
                                'time': t_str
                            })
                
                final['threats'] = len(feed)
                sorted_countries = sorted(country_stats.items(), key=lambda x: x[1], reverse=True)[:5]
                final['top_countries'] = [{'code': k, 'count': v} for k, v in sorted_countries]
                
            except:
                pass
                
        return final
    
    def _run(self):
        while not self._should_stop():
            try:
                http = self.get_http_totals()
                events = self.get_raw_events()
                
                if http or events:
                    data = self.process_data(http, events)
                    self.log(f"Req: {data['total_requests']} | Blocked: {data['blocked']}")
                    requests.post(self.esp_url, json=data, timeout=5)
                else:
                    self.log("No data from Cloudflare")
            except requests.exceptions.RequestException:
                self.log("ESP32 unreachable")
            except Exception as e:
                self.log(f"Error: {str(e)[:30]}")
            
            if self._sleep(30):
                break

def sanitize_for_display(text):
    text = re.sub(r'<[^>]+>', '', text)
    text = html.unescape(text)
    text = text.replace('\u0142', 'l').replace('\u0141', 'L')  # ł/Ł
    text = unicodedata.normalize('NFKD', text)
    text = text.encode('ascii', 'ignore').decode('ascii')
    text = text.replace('\n', ' ').replace('\r', '')
    text = re.sub(r'\s+', ' ', text).strip()
    return text


class YouTubeProxyService(BaseService):
    
    def __init__(self, log_callback=None):
        super().__init__(log_callback)
        self.esp_url = f"http://{ESP32_IP}/update_youtube"
        self.api_key = YT_API_KEY
        self.channel_id = YT_CHANNEL_ID
        self.uploads_playlist_id = None
        self.channel_name = ''
        self.cached_comments = []
    
    def get_channel_info(self):
        parts = 'statistics,snippet'
        if not self.uploads_playlist_id:
            parts += ',contentDetails'
        
        url = f"{YT_API_BASE}/channels"
        params = {
            'part': parts,
            'id': self.channel_id,
            'key': self.api_key
        }
        r = requests.get(url, params=params, timeout=10)
        data = r.json()
        
        if 'error' in data:
            msg = data.get('error', {}).get('message', 'Unknown error')
            self.log(f"API: {msg[:35]}")
            return None
        
        items = data.get('items', [])
        if not items:
            self.log("Channel not found")
            return None
        
        item = items[0]
        stats = item.get('statistics', {})
        snippet = item.get('snippet', {})
        
        if not self.uploads_playlist_id:
            content = item.get('contentDetails', {})
            self.uploads_playlist_id = content.get('relatedPlaylists', {}).get('uploads', '')
            self.log(f"Uploads playlist: {self.uploads_playlist_id[:20]}")
        
        self.channel_name = sanitize_for_display(snippet.get('title', ''))[:24]
        
        subs = int(stats.get('subscriberCount', 0))
        views = int(stats.get('viewCount', 0))
        vids = int(stats.get('videoCount', 0))
        self.log(f"Channel: {self.channel_name} | Subs: {subs}")
        
        return {
            'subscribers': subs,
            'total_views': views,
            'video_count': vids,
        }
    
    def get_channel_comments(self):
        url = f"{YT_API_BASE}/commentThreads"
        params = {
            'part': 'snippet',
            'allThreadsRelatedToChannelId': self.channel_id,
            'maxResults': 5,
            'order': 'time',
            'key': self.api_key
        }
        
        try:
            r = requests.get(url, params=params, timeout=10)
            data = r.json()
            
            if 'error' in data:
                reason = data.get('error', {}).get('errors', [{}])[0].get('reason', 'unknown')
                self.log(f"Comments: {reason[:25]}")
                return self.cached_comments
            
            comments = []
            for item in data.get('items', []):
                snippet = item['snippet']['topLevelComment']['snippet']
                
                text = sanitize_for_display(snippet.get('textDisplay', ''))
                if not text.strip():
                    text = '(emoji)'
                author = sanitize_for_display(snippet.get('authorDisplayName', 'Unknown'))
                published = snippet.get('publishedAt', '')
                time_str = self.format_time_ago(published)
                
                comments.append({
                    'author': author[:18],
                    'text': text[:85],
                    'time': time_str
                })
            
            return comments
        except Exception as e:
            self.log(f"Comments err: {str(e)[:25]}")
            return self.cached_comments
    
    @staticmethod
    def format_time_ago(iso_time):
        try:
            dt = datetime.fromisoformat(iso_time.replace('Z', '+00:00'))
            now = datetime.now(timezone.utc)
            diff = now - dt
            
            if diff.days > 365:
                return f"{diff.days // 365}y"
            elif diff.days > 30:
                return f"{diff.days // 30}mo"
            elif diff.days > 0:
                return f"{diff.days}d"
            elif diff.seconds > 3600:
                return f"{diff.seconds // 3600}h"
            elif diff.seconds > 60:
                return f"{diff.seconds // 60}m"
            else:
                return "now"
        except:
            return "--"
    
    def _run(self):
        if not self.api_key or not self.channel_id:
            self.log("Missing YOUTUBE_API_KEY or CHANNEL_ID")
            self.state = ServiceState.ERROR
            return
        
        self.log("Connecting to YouTube API...")
        self.log(f"Channel ID: {self.channel_id[:16]}...")
        comment_cycle = 0
        
        while not self._should_stop():
            try:
                stats = self.get_channel_info()
                
                if stats:
                    comment_cycle += 1
                    if comment_cycle >= 3 or not self.cached_comments:
                        self.log("Fetching channel comments...")
                        comments = self.get_channel_comments()
                        if comments:
                            self.cached_comments = comments
                            self.log(f"Got {len(comments)} comments (channel-wide)")
                        comment_cycle = 0
                    
                    data = {
                        'subscribers': stats['subscribers'],
                        'total_views': stats['total_views'],
                        'video_count': stats['video_count'],
                        'channel_name': self.channel_name,
                        'comments': self.cached_comments
                    }
                    
                    requests.post(self.esp_url, json=data, timeout=5)
                    self.log(f"Sent -> Subs: {stats['subscribers']} | Vids: {stats['video_count']}")
                else:
                    self.log("No data from YouTube API")
                    
            except requests.exceptions.RequestException:
                self.log("ESP32 unreachable")
            except Exception as e:
                self.log(f"Error: {str(e)[:30]}")
            
            if self._sleep(90):
                break


# --- CAMERA SERVER SERVICE ---
class CameraServerService(BaseService):
    """RTSP to MJPEG proxy server for IP cameras"""
    
    def __init__(self, log_callback=None):
        super().__init__(log_callback)
        self.last_frame = None
        self.frame_lock = threading.Lock()
        self.cap = None
        self.flask_app = None
        self.server = None
    
    def _run(self):
        self.flask_app = Flask(__name__)
        
        @self.flask_app.route('/snap.jpg')
        def get_snapshot():
            with self.frame_lock:
                if self.last_frame:
                    return Response(self.last_frame, mimetype='image/jpeg')
                else:
                    return "Loading...", 503
        
        from werkzeug.serving import make_server
        try:
            self.server = make_server('0.0.0.0', 5000, self.flask_app, threaded=True)
            flask_thread = threading.Thread(target=self.server.serve_forever, daemon=True)
            flask_thread.start()
            self.log("HTTP server on port 5000")
        except Exception as e:
            self.log(f"Port 5000 busy: {e}")
            return
        
        os.environ["OPENCV_FFMPEG_CAPTURE_OPTIONS"] = "rtsp_transport;tcp"
        self.log(f"Connecting to camera...")
        
        reconnect_delay = 2
        
        while not self._should_stop():
            try:
                if self.cap is None or not self.cap.isOpened():
                    self.cap = cv2.VideoCapture(RTSP_URL)
                    if not self.cap.isOpened():
                        self.log("Cannot connect to camera")
                        if self._sleep(reconnect_delay):
                            break
                        continue
                    self.log("Camera connected")
                
                success, frame = self.cap.read()
                if success:
                    resized = cv2.resize(frame, (1024, 600))
                    ret, buffer = cv2.imencode('.jpg', resized, [cv2.IMWRITE_JPEG_QUALITY, 10])
                    
                    if ret:
                        with self.frame_lock:
                            self.last_frame = buffer.tobytes()
                else:
                    self.cap.release()
                    self.cap = None
                    if self._sleep(1):
                        break
                    continue
                
                time.sleep(0.01)
                
            except Exception as e:
                self.log(f"Error: {str(e)[:30]}")
                if self.cap:
                    self.cap.release()
                    self.cap = None
                if self._sleep(2):
                    break
        
        if self.cap:
            self.cap.release()
            self.cap = None
        
        if self.server:
            self.server.shutdown()
            self.server = None


# --- GUI: SERVICE CARD ---
class ServiceCard(ctk.CTkFrame):
    def __init__(self, parent, name, description, service_class, icon_emoji, on_toggle=None):
        super().__init__(parent, corner_radius=15)
        
        self.name = name
        self.service_class = service_class
        self.on_toggle = on_toggle
        self.service = None
        self._action_lock = threading.Lock()
        self._is_transitioning = False
        
        self.colors = {
            ServiceState.STOPPED: ("#2b2b2b", "#ff4444"),
            ServiceState.STARTING: ("#2b2b2b", "#ffaa00"),
            ServiceState.RUNNING: ("#2b2b2b", "#1DB954"),
            ServiceState.STOPPING: ("#2b2b2b", "#ffaa00"),
            ServiceState.ERROR: ("#2b2b2b", "#ff4444"),
        }
        
        self.configure(fg_color="#2b2b2b", border_width=3, border_color="#2b2b2b")
        self.grid_columnconfigure(0, weight=1)
        
        self.header_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.header_frame.grid(row=0, column=0, padx=20, pady=(20, 5), sticky="w")
        
        self.icon_label = ctk.CTkLabel(self.header_frame, text=icon_emoji, font=ctk.CTkFont(size=32))
        self.icon_label.pack(side="left", padx=(0, 10))
        
        self.name_label = ctk.CTkLabel(self.header_frame, text=name, font=ctk.CTkFont(size=20, weight="bold"))
        self.name_label.pack(side="left")
        
        self.status_frame = ctk.CTkFrame(self.header_frame, fg_color="#ff4444", corner_radius=10)
        self.status_frame.pack(side="left", padx=(15, 0))
        
        self.status_label = ctk.CTkLabel(self.status_frame, text="OFFLINE", font=ctk.CTkFont(size=11, weight="bold"), text_color="white")
        self.status_label.pack(padx=8, pady=2)
        
        self.desc_label = ctk.CTkLabel(self, text=description, font=ctk.CTkFont(size=13), text_color="#888888")
        self.desc_label.grid(row=1, column=0, padx=20, pady=(0, 15), sticky="w")
        
        self.log_text = ctk.CTkTextbox(self, height=60, font=ctk.CTkFont(family="Consolas", size=10), fg_color="#1a1a1a", corner_radius=8)
        self.log_text.grid(row=2, column=0, padx=20, pady=(0, 15), sticky="ew")
        self.log_text.configure(state="disabled")
        
        for widget in [self, self.header_frame, self.icon_label, self.name_label, self.desc_label]:
            widget.bind("<Button-1>", self.on_click)
        
        self.bind("<Enter>", self.on_enter)
        self.bind("<Leave>", self.on_leave)
        
        self._update_status_job = None
    
    def on_enter(self, event):
        state = self.service.state if self.service else ServiceState.STOPPED
        if state in [ServiceState.STOPPED, ServiceState.ERROR]:
            self.configure(fg_color="#3b3b3b")
    
    def on_leave(self, event):
        state = self.service.state if self.service else ServiceState.STOPPED
        if state in [ServiceState.STOPPED, ServiceState.ERROR]:
            self.configure(fg_color="#2b2b2b")
    
    def on_click(self, event=None):
        if not self._action_lock.acquire(blocking=False):
            return
        
        try:
            if self._is_transitioning:
                return
            
            self._is_transitioning = True
            threading.Thread(target=self._toggle_action, daemon=True).start()
            
        finally:
            self._action_lock.release()
    
    def _toggle_action(self):
        try:
            if self.service is None or self.service.state in [ServiceState.STOPPED, ServiceState.ERROR]:
                self._start_service()
            elif self.service.state == ServiceState.RUNNING:
                self._stop_service()
        finally:
            self._is_transitioning = False
            if self.on_toggle:
                self.after(0, lambda: self.on_toggle(self.name, self.is_running()))
    
    def _start_service(self):
        self.after(0, lambda: self._update_ui(ServiceState.STARTING))
        
        self.service = self.service_class(log_callback=self._safe_log)
        if self.service.start():
            self._start_status_monitor()
        else:
            self.after(0, lambda: self._update_ui(ServiceState.ERROR))
    
    def _stop_service(self):
        if self.service:
            self.after(0, lambda: self._update_ui(ServiceState.STOPPING))
            self.service.stop()
            self._stop_status_monitor()
            self.after(0, lambda: self._update_ui(ServiceState.STOPPED))
    
    def _safe_log(self, message):
        try:
            self.after(0, lambda m=message: self._log_message(m))
        except:
            pass
    
    def _log_message(self, message):
        try:
            timestamp = datetime.now().strftime("%H:%M:%S")
            self.log_text.configure(state="normal")
            self.log_text.insert("end", f"[{timestamp}] {message}\n")
            self.log_text.see("end")
            self.log_text.configure(state="disabled")
        except:
            pass
    
    def _update_ui(self, state):
        try:
            bg_color, status_color = self.colors.get(state, ("#2b2b2b", "#ff4444"))
            self.configure(border_color=status_color if state == ServiceState.RUNNING else "#2b2b2b")
            self.status_frame.configure(fg_color=status_color)
            self.status_label.configure(text=state.value)
        except:
            pass
    
    def _start_status_monitor(self):
        def check_status():
            if self.service:
                state = self.service.state
                self._update_ui(state)
                if state in [ServiceState.RUNNING, ServiceState.STARTING]:
                    self._update_status_job = self.after(500, check_status)
        
        self._update_status_job = self.after(500, check_status)
    
    def _stop_status_monitor(self):
        if self._update_status_job:
            try:
                self.after_cancel(self._update_status_job)
            except:
                pass
            self._update_status_job = None
    
    def is_running(self):
        return self.service and self.service.state == ServiceState.RUNNING
    
    def is_busy(self):
        return self._is_transitioning or (self.service and self.service.state in [ServiceState.STARTING, ServiceState.STOPPING])
    
    def force_stop(self):
        self._stop_status_monitor()
        if self.service:
            self.service.stop()


# --- GUI: MAIN WINDOW ---
class ESP32Dashboard(ctk.CTk):
    def __init__(self):
        super().__init__()
        
        self.title("ESP32 Dashboard Controller")
        self.geometry("820x520")
        self.minsize(780, 480)
        
        self.icon_path = os.path.join(SCRIPT_DIR, "icon.ico")
        if os.path.exists(self.icon_path):
            try:
                self.iconbitmap(self.icon_path)
            except:
                pass
        
        self.configure(fg_color="#1a1a1a")
        
        self._all_action_lock = threading.Lock()
        
        self.main_frame = ctk.CTkFrame(self, fg_color="transparent")
        self.main_frame.pack(fill="both", expand=True, padx=20, pady=20)
        
        self.header_frame = ctk.CTkFrame(self.main_frame, fg_color="transparent")
        self.header_frame.pack(fill="x", pady=(0, 15))
        
        self.title_label = ctk.CTkLabel(self.header_frame, text="ESP32 Dashboard", font=ctk.CTkFont(size=28, weight="bold"))
        self.title_label.pack(side="left")
        
        self.all_btn = ctk.CTkButton(
            self.header_frame, text="START ALL", font=ctk.CTkFont(size=12, weight="bold"),
            fg_color="#1DB954", hover_color="#1ed760", width=120, height=35, corner_radius=20,
            command=self.toggle_all
        )
        self.all_btn.pack(side="right")
        
        self.subtitle = ctk.CTkLabel(self.main_frame, text="Click a card to toggle service on/off", font=ctk.CTkFont(size=13), text_color="#666666")
        self.subtitle.pack(anchor="w", pady=(0, 10))
        
        # 2x2 Grid
        self.grid_frame = ctk.CTkFrame(self.main_frame, fg_color="transparent")
        self.grid_frame.pack(fill="both", expand=True)
        self.grid_frame.grid_columnconfigure(0, weight=1)
        self.grid_frame.grid_columnconfigure(1, weight=1)
        self.grid_frame.grid_rowconfigure(0, weight=1)
        self.grid_frame.grid_rowconfigure(1, weight=1)
        
        self.cards = []
        
        card1 = ServiceCard(self.grid_frame, name="Camera Server", description="RTSP to MJPEG proxy (port 5000)", service_class=CameraServerService, icon_emoji="CAM", on_toggle=self.on_service_toggle)
        card1.grid(row=0, column=0, padx=(0, 8), pady=(0, 8), sticky="nsew")
        self.cards.append(card1)
        
        card2 = ServiceCard(self.grid_frame, name="Cloudflare Proxy", description="Security analytics to ESP32", service_class=CloudflareProxyService, icon_emoji="CF", on_toggle=self.on_service_toggle)
        card2.grid(row=0, column=1, padx=(8, 0), pady=(0, 8), sticky="nsew")
        self.cards.append(card2)
        
        card3 = ServiceCard(self.grid_frame, name="PC Monitor", description="CPU/RAM/Network to ESP32", service_class=PCMonitorService, icon_emoji="PC", on_toggle=self.on_service_toggle)
        card3.grid(row=1, column=0, padx=(0, 8), pady=(8, 0), sticky="nsew")
        self.cards.append(card3)
        
        card4 = ServiceCard(self.grid_frame, name="YouTube Stats", description="YT channel stats & comments", service_class=YouTubeProxyService, icon_emoji="YT", on_toggle=self.on_service_toggle)
        card4.grid(row=1, column=1, padx=(8, 0), pady=(8, 0), sticky="nsew")
        self.cards.append(card4)
        
        self.footer = ctk.CTkLabel(self.main_frame, text="ESP32 Nexus Dash Controller", font=ctk.CTkFont(size=11), text_color="#444444")
        self.footer.pack(side="bottom", pady=(20, 0))
        
        self.after(1000, self.start_all_services)
        
        self.protocol("WM_DELETE_WINDOW", self.hide_to_tray)
        self.bind("<Unmap>", self.on_unmap)
        
        self.tray_icon = None
        self.setup_tray_icon()
        
        self.after(50, self.hide_to_tray)
        
    def setup_tray_icon(self):
        try:
            image = Image.open(self.icon_path) if os.path.exists(self.icon_path) else Image.new('RGB', (64, 64), color='black')
        except:
            image = Image.new('RGB', (64, 64), color='black')
            
        menu = pystray.Menu(
            item('Otwórz', self.show_from_tray, default=True),
            item('Zakończ', self.quit_app)
        )
        
        self.tray_icon = pystray.Icon("ESP32Dashboard", image, "ESP32 Dashboard", menu)
        threading.Thread(target=self.tray_icon.run, daemon=True).start()

    def on_unmap(self, event):
        try:
            if str(event.widget) == str(self) and self.state() == 'iconic':
                self.after(10, self.hide_to_tray)
        except:
            pass
    
    def hide_to_tray(self):
        self.withdraw()
            
    def show_from_tray(self, icon=None, item=None):
        self.after(0, self.restore_window)
        
    def restore_window(self):
        self.deiconify()       
        self.state('normal')  
        self.lift()             
        self.focus_force()      
        
    def quit_app(self, icon=None, item=None):
        if self.tray_icon:
            self.tray_icon.stop()
        self.after(0, self._perform_quit)
        
    def _perform_quit(self):
        for card in self.cards:
            card.force_stop()
        time.sleep(0.5)
        self.destroy()
        os._exit(0) 
    
    def start_all_services(self):
        def start_next(index=0):
            if index < len(self.cards):
                card = self.cards[index]
                if not card.is_running() and not card.is_busy():
                    card.on_click()
                    self.after(500, lambda: start_next(index + 1))
                else:
                    self.after(100, lambda: start_next(index + 1))
            else:
                self.update_all_button()
        
        start_next()
    
    def toggle_all(self):
        if not self._all_action_lock.acquire(blocking=False):
            return
        
        try:
            if any(card.is_busy() for card in self.cards):
                return
            
            all_running = all(card.is_running() for card in self.cards)
            
            if all_running:
                for card in self.cards:
                    if card.is_running():
                        threading.Thread(target=card._stop_service, daemon=True).start()
            else:
                self.start_all_services()
        finally:
            self._all_action_lock.release()
    
    def update_all_button(self):
        try:
            all_running = all(card.is_running() for card in self.cards)
            if all_running:
                self.all_btn.configure(text="STOP ALL", fg_color="#ff4444", hover_color="#ff6666")
            else:
                self.all_btn.configure(text="START ALL", fg_color="#1DB954", hover_color="#1ed760")
        except:
            pass
    
    def on_service_toggle(self, name, is_active):
        self.after(100, self.update_all_button)


def main():
    multiprocessing.freeze_support()
    app = ESP32Dashboard()
    app.mainloop()


if __name__ == "__main__":
    main()