import time
import sys
import os
from math import sin, cos, sqrt, atan2, radians, degrees, atan, pi
from PIL import Image, ImageDraw, ImageFont

# Import existing libraries
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'library'))
from library import ST7789
from library.config import DEBUG_MODE

# Import service classes
from .gps_service import GPSService
from .traffic_service import TrafficService
from .wifi_service import WiFiService
from .attitude_service import AttitudeService


class PilotAssistantSystem:
    """Main system coordinator"""
    def __init__(self):
        # Initialize display
        self.lcd = ST7789.ST7789()
        self.lcd.Init()
        self.lcd.clear()
        self.lcd.bl_DutyCycle(50)

        # Splash screen
        image = Image.open('./images/output.png')
        im_r = image.rotate(270)
        self.lcd.ShowImage(im_r)
        time.sleep(2)
        
        # Initialize services
        self.gps_service = GPSService()
        self.traffic_service = TrafficService(self.gps_service)
        self.wifi_service = WiFiService()
        self.attitude_service = AttitudeService()
        
        # Attitude warning thresholds (degrees)
        self.max_bank_angle = 45.0  # Maximum safe bank angle
        self.max_pitch_angle = 20.0  # Maximum safe pitch angle (up or down)
        
        # Blinking state for warnings
        self.blink_state = False
        self.last_blink_time = 0
        self.blink_interval = 0.5  # Blink every 500ms
        
        # Display state
        self.current_display = 'main'  # main, attitude, traffic, camera
        
        # Fonts
        self.font_large = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 24)
        self.font_medium = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf', 18)
        self.font_small = ImageFont.truetype('/usr/share/fonts/truetype/freefont/FreeMono.ttf', 14)
        
        # Button states
        self.last_key1_state = self.lcd.digital_read(self.lcd.GPIO_KEY1_PIN)
        self.last_key2_state = self.lcd.digital_read(self.lcd.GPIO_KEY2_PIN)
        self.last_key3_state = self.lcd.digital_read(self.lcd.GPIO_KEY3_PIN)
        self.last_center_state = self.lcd.digital_read(self.lcd.GPIO_KEY_PRESS_PIN)
        
        # Traffic display state
        self.last_up_state = self.lcd.digital_read(self.lcd.GPIO_KEY_UP_PIN)
        self.last_down_state = self.lcd.digital_read(self.lcd.GPIO_KEY_DOWN_PIN)
        self.show_aircraft_info = False
        self.aircraft_info_timeout = 0
        self.selected_aircraft = None
        self.current_aircraft_index = 0
        self.sorted_aircraft_list = []
    
    def start_services(self):
        """Start all background services"""
        self.gps_service.start()
        self.traffic_service.start()
        self.wifi_service.start()
        self.attitude_service.start()
        print("All services started")
    
    def stop_services(self):
        """Stop all background services"""
        self.gps_service.stop()
        self.traffic_service.stop()
        self.wifi_service.stop()
        self.attitude_service.stop()
        print("All services stopped")
    
    def handle_buttons(self):
        """Handle button presses for display switching"""
        key1_state = self.lcd.digital_read(self.lcd.GPIO_KEY1_PIN)
        key2_state = self.lcd.digital_read(self.lcd.GPIO_KEY2_PIN)
        key3_state = self.lcd.digital_read(self.lcd.GPIO_KEY3_PIN)
        center_state = self.lcd.digital_read(self.lcd.GPIO_KEY_PRESS_PIN)
        
        # KEY1 - Attitude Indicator
        if key1_state == 0 and self.last_key1_state == 1:
            time.sleep(0.05)  # Debounce
            if self.lcd.digital_read(self.lcd.GPIO_KEY1_PIN) == 0:
                self.current_display = 'attitude'
                print("Switched to attitude display")
        
        # KEY2 - Traffic Radar
        if key2_state == 0 and self.last_key2_state == 1:
            time.sleep(0.05)  # Debounce
            if self.lcd.digital_read(self.lcd.GPIO_KEY2_PIN) == 0:
                self.current_display = 'traffic'
                print("Switched to traffic display")
        
        # KEY3 - Camera
        if key3_state == 0 and self.last_key3_state == 1:
            time.sleep(0.05)  # Debounce
            if self.lcd.digital_read(self.lcd.GPIO_KEY3_PIN) == 0:
                self.current_display = 'camera'
                print("Switched to camera display")
        
        # CENTER - Back to main
        if center_state == 0 and self.last_center_state == 1:
            time.sleep(0.05)  # Debounce
            if self.lcd.digital_read(self.lcd.GPIO_KEY_PRESS_PIN) == 0:
                self.current_display = 'main'
                print("Switched to main display")
        
        # Update button states
        self.last_key1_state = key1_state
        self.last_key2_state = key2_state
        self.last_key3_state = key3_state
        self.last_center_state = center_state
    
    def update_blink_state(self):
        """Update blinking state for warnings"""
        current_time = time.time()
        if current_time - self.last_blink_time >= self.blink_interval:
            self.blink_state = not self.blink_state
            self.last_blink_time = current_time
    
    def display_main(self):
        """Display main status screen"""
        # Use light red background in debug mode, black otherwise
        bg_color = "#400000" if DEBUG_MODE else "BLACK"  # Dark red background in debug mode
        background = Image.new("RGB", (self.lcd.width, self.lcd.height), bg_color)
        draw = ImageDraw.Draw(background)
        
        # Get data from services
        gps_data = self.gps_service.get_data()
        aircraft_count = len(self.traffic_service.get_data())
        wifi_status = self.wifi_service.get_status()
        attitude_data = self.attitude_service.get_attitude()
        
        # Title with ribbon background
        title_text = "PILOT ASSISTANT"
        title_bbox = draw.textbbox((0, 0), title_text, font=self.font_large)
        title_width = title_bbox[2] - title_bbox[0]
        title_height = title_bbox[3] - title_bbox[1]
        
        # Draw ribbon background (full width)
        draw.rectangle((0, 5, self.lcd.width, title_height + 15), fill="CYAN")
        
        # Center the title text on the ribbon
        title_x = (self.lcd.width - title_width) // 2
        draw.text((title_x, 8), title_text, fill="BLACK", font=self.font_large)
        
        # Status information
        y_pos = 50
        
        # GPS Status
        gps_color = "GREEN" if gps_data['gps_status'] == "GREEN" else "RED"
        gps_text = "OK" if gps_data['gps_status'] == "GREEN" else "N/A"
        draw.text((10, y_pos), "GPS:", fill="WHITE", font=self.font_large)
        draw.text((80, y_pos), gps_text, fill=gps_color, font=self.font_large)
        y_pos += 35
        
        # WiFi Status
        wifi_color = "GREEN" if wifi_status else "RED"
        wifi_text = "OK" if wifi_status else "FAIL"
        draw.text((10, y_pos), "WIFI:", fill="WHITE", font=self.font_large)
        draw.text((90, y_pos), wifi_text, fill=wifi_color, font=self.font_large)
        y_pos += 35
        
        # Aircraft count
        draw.text((10, y_pos), "AIRCRAFT:", fill="WHITE", font=self.font_large)
        draw.text((140, y_pos), str(aircraft_count), fill="YELLOW", font=self.font_large)
        y_pos += 35
        
        # Attitude data with warnings
        pitch = attitude_data['pitch']
        roll = attitude_data['roll']
        
        # Update blinking state
        self.update_blink_state()
        
        # Check for excessive pitch (nose up/down)
        pitch_warning = abs(pitch) > self.max_pitch_angle
        pitch_color = "RED" if pitch_warning and self.blink_state else "WHITE"
        pitch_text = f"PITCH: {pitch:.1f}°"
        if pitch_warning and self.blink_state:
            pitch_text = f"⚠ {pitch_text} ⚠"
        
        draw.text((10, y_pos), pitch_text, fill=pitch_color, font=self.font_large)
        y_pos += 30
        
        # Check for excessive bank (roll left/right)
        bank_warning = abs(roll) > self.max_bank_angle
        bank_color = "RED" if bank_warning and self.blink_state else "WHITE"
        roll_text = f"ROLL: {roll:.1f}°"
        if bank_warning and self.blink_state:
            roll_text = f"⚠ {roll_text} ⚠"
        
        draw.text((10, y_pos), roll_text, fill=bank_color, font=self.font_large)
        y_pos += 30
        
        # Large center-screen warnings
        if (pitch_warning or bank_warning) and self.blink_state:
            center_x = self.lcd.width // 2
            center_y = self.lcd.height // 2
            
            if pitch_warning:
                warning_text = "PITCH ANGLE!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=self.font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                draw.rectangle((center_x - warning_width//2 - 10, center_y - 20, 
                              center_x + warning_width//2 + 10, center_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, center_y - 12), warning_text, fill="WHITE", font=self.font_large)
            
            if bank_warning:
                warning_text = "BANK ANGLE!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=self.font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                warning_y = center_y + 30 if pitch_warning else center_y
                draw.rectangle((center_x - warning_width//2 - 10, warning_y - 20, 
                              center_x + warning_width//2 + 10, warning_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, warning_y - 12), warning_text, fill="WHITE", font=self.font_large)
                
        # Display image
        im_r = background.rotate(270)
        self.lcd.ShowImage(im_r)
    
    def lat_lon_to_xy(self, lat, lon, center_lat, center_lon, center_y=130, scale=2.0):
        """Convert lat/lon to x,y coordinates for display (centered on user position)"""
        # Simple Mercator-like projection for small areas
        x = (lon - center_lon) * 111 * cos(radians(center_lat)) * scale + 120
        y = (center_lat - lat) * 111 * scale + center_y  # Inverted Y for screen coordinates
        return int(x), int(y)
    
    def handle_traffic_controls(self):
        """Handle UP/DOWN controls for aircraft selection in traffic mode"""
        if self.current_display != 'traffic':
            return
        
        # Check for UP button - next aircraft
        up_state = self.lcd.digital_read(self.lcd.GPIO_KEY_UP_PIN)
        if up_state == 0 and self.last_up_state == 1:
            time.sleep(0.05)  # Debounce
            if self.lcd.digital_read(self.lcd.GPIO_KEY_UP_PIN) == 0:
                # Update aircraft list only when button is pressed
                aircraft_list = self.traffic_service.get_data()
                if aircraft_list:
                    self.sorted_aircraft_list = sorted(aircraft_list, key=lambda ac: ac['distance_km'])
                    # Go to next aircraft (forward in list)
                    self.current_aircraft_index = (self.current_aircraft_index + 1) % len(self.sorted_aircraft_list)
                    self.selected_aircraft = self.sorted_aircraft_list[self.current_aircraft_index]
                    self.show_aircraft_info = True
                    self.aircraft_info_timeout = time.time() + 15  # Show for 15 seconds
                    print(f"Selected aircraft: {self.selected_aircraft['callsign']} at {self.selected_aircraft['distance_km']:.1f}km")
        self.last_up_state = up_state
        
        # Check for DOWN button - previous aircraft
        down_state = self.lcd.digital_read(self.lcd.GPIO_KEY_DOWN_PIN)
        if down_state == 0 and self.last_down_state == 1:
            time.sleep(0.05)  # Debounce
            if self.lcd.digital_read(self.lcd.GPIO_KEY_DOWN_PIN) == 0:
                # Update aircraft list only when button is pressed
                aircraft_list = self.traffic_service.get_data()
                if aircraft_list:
                    self.sorted_aircraft_list = sorted(aircraft_list, key=lambda ac: ac['distance_km'])
                    # Go to previous aircraft (backward in list)
                    self.current_aircraft_index = (self.current_aircraft_index - 1) % len(self.sorted_aircraft_list)
                    self.selected_aircraft = self.sorted_aircraft_list[self.current_aircraft_index]
                    self.show_aircraft_info = True
                    self.aircraft_info_timeout = time.time() + 15  # Show for 15 seconds
                    print(f"Selected aircraft: {self.selected_aircraft['callsign']} at {self.selected_aircraft['distance_km']:.1f}km")
        self.last_down_state = down_state
        
        # Hide aircraft info after timeout
        if self.show_aircraft_info and time.time() > self.aircraft_info_timeout:
            self.show_aircraft_info = False
    
    def display_traffic(self):
        """Display full traffic radar with aircraft positions and selection"""
        # Get data from services
        gps_data = self.gps_service.get_data()
        aircraft_list = self.traffic_service.get_data()
        wifi_status = self.wifi_service.get_status()
        
        # Get GPS coordinates for display (background service handles API calls)
        user_lat = user_lon = None
        if gps_data['gps_data'] and gps_data['gps_data']['latitude'] and gps_data['gps_data']['longitude']:
            user_lat = gps_data['gps_data']['latitude']
            user_lon = gps_data['gps_data']['longitude']
            # Note: Background traffic service handles periodic API updates
        
        background = Image.new("RGB", (self.lcd.width, self.lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        
        # GPS Status - red when using IP location, green when GPS coordinates available
        gps_color = "GREEN" if gps_data['gps_status'] == "GREEN" else "RED"
        draw.text((10, 5), "GPS", fill=gps_color, font=self.font_large)
        
        # WiFi Status - positioned top right
        wifi_color = "GREEN" if wifi_status else "RED"
        wifi_text = "WIFI"
        # Calculate text width to position from right edge
        wifi_bbox = draw.textbbox((0, 0), wifi_text, font=self.font_large)
        wifi_width = wifi_bbox[2] - wifi_bbox[0]
        wifi_x = self.lcd.width - wifi_width - 10  # 10 pixels from right edge
        draw.text((wifi_x, 5), wifi_text, fill=wifi_color, font=self.font_large)
        
        # Always draw radar display
        center_x, center_y = 120, 130
        draw.ellipse([center_x-100, center_y-100, center_x+100, center_y+100], outline="DARKGRAY")  # ~40km
        draw.ellipse([center_x-50, center_y-50, center_x+50, center_y+50], outline="DARKGRAY")  # ~20km
        
        # Draw user position as bigger white aircraft symbol
        aircraft_points = [
            (center_x, center_y - 6),  # nose
            (center_x - 3, center_y + 3),  # left wing
            (center_x - 2, center_y + 5),  # left tail
            (center_x, center_y + 3),   # center tail
            (center_x + 2, center_y + 5),  # right tail
            (center_x + 3, center_y + 3),  # right wing
        ]
        draw.polygon(aircraft_points, fill="WHITE")
        
        # Draw aircraft as dots with direction indicators
        if aircraft_list:
            # Use GPS location if available, otherwise use fallback for display
            display_lat = display_lon = None
            if user_lat and user_lon:
                display_lat, display_lon = user_lat, user_lon
            else:
                # Use fallback location for display purposes (debug coordinates if enabled)
                display_lat, display_lon = self.traffic_service.get_fallback_location()
            
            if display_lat and display_lon:
                for aircraft in aircraft_list:
                    ac_x, ac_y = self.lat_lon_to_xy(aircraft['lat'], aircraft['lon'], display_lat, display_lon, center_y)
                    
                    # Only draw aircraft that are within display bounds
                    if 10 <= ac_x <= 230 and 10 <= ac_y <= 230:
                        # Check if this is the selected aircraft
                        if self.show_aircraft_info and self.selected_aircraft and aircraft['callsign'] == self.selected_aircraft['callsign']:
                            # Draw selected aircraft as red dot
                            dot_color = "RED"
                            line_color = "RED"
                        else:
                            # Draw aircraft as green dot
                            dot_color = "GREEN"
                            line_color = "GREEN"
                        
                        # Draw aircraft dot
                        draw.ellipse([ac_x-6, ac_y-6, ac_x+6, ac_y+6], fill=dot_color)
                        
                        # Draw direction line (heading indicator)
                        if aircraft['heading'] is not None:
                            line_length = 12
                            end_x = ac_x + line_length * sin(radians(aircraft['heading']))
                            end_y = ac_y - line_length * cos(radians(aircraft['heading']))
                            draw.line([ac_x, ac_y, end_x, end_y], fill=line_color, width=2)
        
        # Show selected aircraft info at bottom left
        if self.show_aircraft_info and self.selected_aircraft:
            # Position at bottom left - moved higher for better visibility
            info_x = 10
            info_y = 160
            
            # Get text values
            callsign_text = f"{self.selected_aircraft['callsign']}"
            altitude_m = self.selected_aircraft['altitude']
            altitude_ft = int(altitude_m * 3.28084) if altitude_m else 0
            altitude_text = f"{altitude_ft}ft"
            speed_text = f"{self.selected_aircraft['speed_knots']}kts"
            
            # Draw the texts aligned to bottom left
            draw.text((info_x, info_y), callsign_text, fill="CYAN", font=self.font_large)
            draw.text((info_x, info_y + 30), altitude_text, fill="CYAN", font=self.font_large)
            draw.text((info_x, info_y + 60), speed_text, fill="CYAN", font=self.font_large)
        
        # Display image
        im_r = background.rotate(270)
        self.lcd.ShowImage(im_r)
    
    def display_attitude(self):
        """Display artificial horizon attitude indicator (based on gyro_menu)"""
        # Get attitude data
        attitude_data = self.attitude_service.get_attitude()
        pitch = attitude_data['pitch']
        roll = attitude_data['roll']
        
        background = Image.new("RGB", (self.lcd.width, self.lcd.height), "BLACK")
        draw = ImageDraw.Draw(background)
        
        # Update blinking state
        self.update_blink_state()
        
        # Use the same drawing function as gyro_menu
        self.draw_artificial_horizon(draw, self.lcd.width, self.lcd.height, pitch, roll)
        
        # Check for warnings
        pitch_warning = abs(pitch) > self.max_pitch_angle
        bank_warning = abs(roll) > self.max_bank_angle
        
        # Display pitch values with warning
        pitch_color = "RED" if pitch_warning and self.blink_state else "YELLOW"
        pitch_label = "⚠ Pitch" if pitch_warning and self.blink_state else "Pitch"
        pitch_value = f"⚠ {pitch:.1f}°" if pitch_warning and self.blink_state else f"{pitch:.1f}°"
        
        draw.text((5, 5), pitch_label, fill=pitch_color, font=self.font_medium)
        draw.text((5, 30), pitch_value, fill=pitch_color, font=self.font_medium)
        
        # Display roll values with warning (right-aligned)
        roll_color = "RED" if bank_warning and self.blink_state else "YELLOW"
        roll_label = "⚠ Roll" if bank_warning and self.blink_state else "Roll"
        roll_value = f"⚠ {roll:.1f}°" if bank_warning and self.blink_state else f"{roll:.1f}°"
        
        roll_bbox = draw.textbbox((0, 0), roll_label, font=self.font_medium)
        roll_width = roll_bbox[2] - roll_bbox[0]
        value_bbox = draw.textbbox((0, 0), roll_value, font=self.font_medium)
        value_width = value_bbox[2] - value_bbox[0]
        
        draw.text((self.lcd.width - roll_width - 5, 5), roll_label, fill=roll_color, font=self.font_medium)
        draw.text((self.lcd.width - value_width - 5, 30), roll_value, fill=roll_color, font=self.font_medium)
        
        # Large center-screen warnings on attitude display
        if (pitch_warning or bank_warning) and self.blink_state:
            center_x = self.lcd.width // 2
            center_y = self.lcd.height // 2
            
            if pitch_warning:
                warning_text = "PITCH ANGLE!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=self.font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                draw.rectangle((center_x - warning_width//2 - 10, center_y - 20, 
                              center_x + warning_width//2 + 10, center_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, center_y - 12), warning_text, fill="WHITE", font=self.font_large)
            
            if bank_warning:
                warning_text = "BANK ANGLE!"
                warning_bbox = draw.textbbox((0, 0), warning_text, font=self.font_large)
                warning_width = warning_bbox[2] - warning_bbox[0]
                warning_y = center_y + 30 if pitch_warning else center_y
                draw.rectangle((center_x - warning_width//2 - 10, warning_y - 20, 
                              center_x + warning_width//2 + 10, warning_y + 20), fill="RED")
                draw.text((center_x - warning_width//2, warning_y - 12), warning_text, fill="WHITE", font=self.font_large)
        
        # Instructions
        draw.text((10, 220), "CENTER: Back", fill="CYAN", font=self.font_small)
        
        # Display image
        im_r = background.rotate(270)
        self.lcd.ShowImage(im_r)
    
    def draw_artificial_horizon(self, draw, width, height, pitch, roll):
        """Draw artificial horizon with pitch and roll indication using full screen (from gyro_menu)"""
        
        # Parameters
        pitch_scale = 3  # pixels per degree
        roll_rad = radians(roll)
        
        # Calculate pitch offset (move horizon line up/down)
        pitch_offset = pitch * pitch_scale
        
        # Create horizon line across full width
        line_length = width
        x1_local = -line_length
        y1_local = pitch_offset
        x2_local = line_length
        y2_local = pitch_offset
        
        # Rotate horizon line by roll
        cos_r = cos(roll_rad)
        sin_r = sin(roll_rad)
        
        def rotate(x, y):
            return (
                x * cos_r - y * sin_r,
                x * sin_r + y * cos_r
            )
        
        x1_rot, y1_rot = rotate(x1_local, y1_local)
        x2_rot, y2_rot = rotate(x2_local, y2_local)
        
        # Center on screen
        cx = width // 2
        cy = height // 2
        x1_screen = x1_rot + cx
        y1_screen = y1_rot + cy
        x2_screen = x2_rot + cx
        y2_screen = y2_rot + cy
        
        # Draw sky background
        draw.rectangle((0, 0, width, height), fill="#4A90E2")
        
        # Draw ground polygon below the horizon
        ground_poly = [
            (0, height),
            (width, height),
            (x2_screen, y2_screen),
            (x1_screen, y1_screen)
        ]
        draw.polygon(ground_poly, fill="#8B4513")
        
        # Draw horizon line
        draw.line([(x1_screen, y1_screen), (x2_screen, y2_screen)], fill="WHITE", width=3)
        
        # Draw pitch ladder marks
        for pitch_mark in [-30, -20, -10, 10, 20, 30]:
            offset = - (pitch_mark - pitch) * pitch_scale
            mark_length = 30 if pitch_mark % 20 == 0 else 20
            x1, y1 = rotate(-mark_length, offset)
            x2, y2 = rotate(mark_length, offset)
            x1 += cx
            y1 += cy
            x2 += cx
            y2 += cy
            
            # Only draw if within screen bounds
            if 0 <= y1 <= height and 0 <= y2 <= height:
                draw.line([(x1, y1), (x2, y2)], fill="WHITE", width=2)
        
        # Draw roll indicator triangle at top
        triangle_size = 6
        top_x = cx + (height // 2 - 20) * sin(roll_rad)
        top_y = 20 - (height // 2 - 20) * cos(roll_rad)
        draw.polygon([
            (top_x, top_y - triangle_size),
            (top_x - triangle_size, top_y + triangle_size),
            (top_x + triangle_size, top_y + triangle_size)
        ], fill="YELLOW", outline="CYAN")
        
        # Draw aircraft symbol in center
        draw.line([(cx - 20, cy), (cx - 5, cy)], fill="YELLOW", width=3)
        draw.line([(cx + 5, cy), (cx + 20, cy)], fill="YELLOW", width=3)
        draw.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill="YELLOW")
    
    def run(self):
        """Main system loop"""
        print("Starting Pilot Assistant System...")
        
        try:
            # Start all background services
            self.start_services()
            
            # Main display loop
            while True:
                # Handle button presses
                self.handle_buttons()
                
                # Handle traffic-specific controls (UP/DOWN for aircraft selection)
                self.handle_traffic_controls()
                
                # Update display based on current mode
                if self.current_display == 'main':
                    self.display_main()
                elif self.current_display == 'traffic':
                    self.display_traffic()
                elif self.current_display == 'attitude':
                    self.display_attitude()
                elif self.current_display == 'camera':
                    # Placeholder for camera display
                    background = Image.new("RGB", (self.lcd.width, self.lcd.height), "BLACK")
                    draw = ImageDraw.Draw(background)
                    draw.text((10, 100), "CAMERA DISPLAY", fill="CYAN", font=self.font_large)
                    draw.text((10, 200), "CENTER: Back", fill="CYAN", font=self.font_small)
                    im_r = background.rotate(270)
                    self.lcd.ShowImage(im_r)
                
                time.sleep(0.1)  # 10 FPS display update
                
        except KeyboardInterrupt:
            print("Shutting down...")
        finally:
            self.stop_services()
            self.lcd.clear()