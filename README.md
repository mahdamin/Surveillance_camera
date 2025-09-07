# Surveillance_camera


# 🎥 ESP32-CAM Internet Surveillance System

A high-performance surveillance camera system built with ESP32-CAM that supports both local network access and global internet access with real-time streaming and video recording capabilities.

## 📋 Table of Contents

- [Features](#-features)
- [Hardware Requirements](#-hardware-requirements)
- [Software Requirements](#-software-requirements)
- [Installation Guide](#-installation-guide)
- [Network Configuration](#-network-configuration)
- [Router Setup for Internet Access](#-router-setup-for-internet-access)
- [Usage Guide](#-usage-guide)
- [Security Configuration](#-security-configuration)
- [Troubleshooting](#-troubleshooting)
- [Technical Specifications](#-technical-specifications)
- [API Endpoints](#-api-endpoints)
- [Contributing](#-contributing)

## 🚀 Features

### Core Functionality
- **Real-time Video Streaming** - High FPS streaming optimized for performance
- **Video Recording** - Automatic MJPEG video recording with file segmentation
- **Dual Access Modes** - Both local network and internet access
- **Storage Management** - Automatic old file deletion when storage is full
- **Performance Monitoring** - Real-time FPS, memory, and storage statistics

### Advanced Features
- **Static IP Configuration** - Stable network addressing
- **Public IP Detection** - Automatic detection and display of external IP
- **HTTP Authentication** - Basic security for remote access
- **Responsive Web Interface** - Modern, mobile-friendly control panel
- **Multi-core Processing** - Optimized task distribution across ESP32 cores
- **Adaptive Quality** - Different quality settings for streaming vs recording

## 🔧 Hardware Requirements

| Component | Specification | Notes |
|-----------|---------------|--------|
| **ESP32-CAM** | AI-Thinker ESP32-CAM | With OV2640 camera module |
| **MicroSD Card** | 4GB+ Class 10 | For video storage |
| **USB-Serial Adapter** | 3.3V/5V compatible | For programming |
| **Power Supply** | 5V 2A minimum | Stable power is crucial |
| **Jumper Wires** | Male-to-female | For connections |

### ESP32-CAM Pinout
```
    ESP32-CAM (AI-Thinker)
    ┌─────────────────────┐
    │  [ANT]         [SD] │
    │                     │
    │  VCC  GND  GPIO0    │
    │  U0R  U0T  GPIO16   │
    │  GPIO4      RESET   │
    └─────────────────────┘
```

## 💻 Software Requirements

- **Arduino IDE** (version 1.8.19 or later)
- **ESP32 Board Package** (version 2.0.0+)
- **Required Libraries:**
  - ESP32 Camera Library (included)
  - AsyncTCP
  - ESPAsyncWebServer
  - ArduinoJson

## 📥 Installation Guide

### 1. Arduino IDE Setup

```bash
# Add ESP32 board manager URL in Arduino IDE:
# File > Preferences > Additional Board Manager URLs
https://dl.espressif.com/dl/package_esp32_index.json
```

### 2. Install Required Libraries

```bash
# In Arduino IDE:
# Tools > Manage Libraries > Search and install:
- AsyncTCP by Hristo Gochkov
- ESPAsyncWebServer by Hristo Gochkov  
- ArduinoJson by Benoit Blanchon
```

### 3. Board Configuration

```
Board: "AI Thinker ESP32-CAM"
Upload Speed: "115200"
Flash Frequency: "80MHz"
Flash Mode: "DIO"
Partition Scheme: "Default 4MB with spiffs"
Core Debug Level: "None"
```

### 4. Programming the ESP32-CAM

**⚠️ Important: ESP32-CAM Programming Mode**

The ESP32-CAM requires manual boot mode switching:

1. **Connect GPIO 0 to GND** before powering on
2. **Power on the ESP32-CAM**
3. **Upload the code**
4. **Disconnect GPIO 0 from GND**
5. **Press RESET** to run

### 5. Wiring Diagram

```
ESP32-CAM    →    USB-Serial Adapter
VCC (5V)     →    5V
GND          →    GND
U0R (GPIO3)  →    TX
U0T (GPIO1)  →    RX

For Programming Mode:
GPIO 0       →    GND (temporarily)
```

## 🌐 Network Configuration

### Static IP Configuration

The system is pre-configured with static IP settings:

```cpp
IPAddress staticIP(192, 168, 1, 253);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
```

**Modify these values** in the code to match your network:

```cpp
// Network Credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// Static IP Configuration
IPAddress staticIP(192, 168, 1, 253);     // Your desired IP
IPAddress gateway(192, 168, 1, 1);        // Your router IP
```

## 🔗 Router Setup for Internet Access

To access your camera from anywhere on the internet:

### 1. Port Forwarding Setup

**Access your router's admin panel** (typically http://192.168.1.1)

**Configure Port Forwarding:**
- **Service Name:** ESP32-CAM
- **External Port:** 8080
- **Internal IP:** 192.168.1.253
- **Internal Port:** 80
- **Protocol:** TCP

### 2. Router Brand-Specific Instructions

#### TP-Link Routers:
```
Advanced → NAT Forwarding → Virtual Servers
→ Add → Service Port: 8080, Internal Port: 80
→ IP Address: 192.168.1.253, Protocol: TCP
```

#### Netgear Routers:
```
Dynamic DNS → Port Forwarding/Port Triggering
→ Add Custom Service → External Port: 8080
→ Internal Port: 80, Server IP: 192.168.1.253
```

#### Linksys Routers:
```
Smart Wi-Fi Tools → Port Forwarding
→ Add new port forwarding rule
→ Device: ESP32-CAM, External Port: 8080, Internal Port: 80
```

### 3. Security Considerations

- Change default ports (use something other than 8080)
- Enable router firewall
- Use strong authentication credentials
- Consider VPN access for enhanced security

## 📱 Usage Guide

### Local Network Access

```
http://192.168.1.253
```

### Internet Access

```
http://YOUR_PUBLIC_IP:8080
```

**The web interface will display your current public IP automatically.**

### Control Interface

#### Main Dashboard
- **Current Mode Display** - Shows IDLE/STREAMING/RECORDING status
- **Performance Metrics** - Real-time FPS, memory usage, storage space
- **Stream Controls** - Start/Stop streaming and recording

#### Available Modes

1. **Streaming Mode**
   - High FPS black & white streaming
   - Optimized for real-time viewing
   - Lower resolution for better performance

2. **Recording Mode**
   - Full color video recording
   - Higher resolution and quality
   - Automatic file segmentation (1-hour chunks)

3. **Idle Mode**
   - Camera standby
   - Low power consumption
   - Ready for mode switching

### Keyboard Shortcuts

- **Stream:** Click "Start Stream" button
- **Record:** Click "Start Recording" button  
- **Stop:** Click "Stop" button (stops all operations)

## 🔐 Security Configuration

### Default Credentials

```cpp
Username: admin
Password: esp32cam
```

**⚠️ Change these immediately:**

```cpp
const char* ACCESS_USERNAME = "your_username";
const char* ACCESS_PASSWORD = "your_strong_password";
```

### Disable Authentication (Optional)

```cpp
bool authenticationEnabled = false;  // Set to false to disable
```

### Additional Security Measures

1. **Use Non-Standard Ports**
   ```cpp
   const int EXTERNAL_PORT = 9999;  // Change from default 8080
   ```

2. **Enable HTTPS** (Advanced)
   - Requires SSL certificate
   - More complex setup but recommended for production

3. **Network Isolation**
   - Place camera on isolated VLAN
   - Restrict unnecessary network access

## 🔍 Troubleshooting

### Common Upload Issues

#### Error: "Failed to connect to ESP32: Wrong boot mode detected"

**Solution:**
1. Connect GPIO 0 to GND before powering on
2. Press RESET button
3. Start upload immediately
4. Remove GPIO 0 connection after upload

#### Error: "Upload failed" or timeout

**Solutions:**
- Reduce upload speed to 115200
- Check power supply (use 5V 2A)
- Verify wiring connections
- Try different USB-Serial adapter

### Network Connection Issues

#### WiFi Connection Failed

**Check:**
- SSID and password are correct
- Router is 2.4GHz (ESP32 doesn't support 5GHz)
- Signal strength is adequate
- Router isn't blocking new devices

#### Can't Access Web Interface

**Troubleshooting:**
1. Verify ESP32 IP address in serial monitor
2. Check if router assigned different IP
3. Disable firewall temporarily
4. Try different browser

### Performance Issues

#### Low FPS Streaming

**Optimizations:**
- Ensure strong WiFi signal
- Close other network-intensive applications
- Check power supply stability
- Reduce image quality settings

#### Storage Issues

**Solutions:**
- Use Class 10 or better microSD card
- Check SD card for errors
- Format SD card as FAT32
- Ensure adequate free space

### Internet Access Issues

#### Can't Access from External Network

**Check:**
- Port forwarding is correctly configured
- Router firewall allows traffic on configured port
- ISP doesn't block the port
- Public IP is correct and current

## 📊 Technical Specifications

### Performance Metrics

| Metric | Value | Notes |
|--------|-------|--------|
| **Max FPS** | 20+ FPS | Depends on network and settings |
| **Resolution** | Up to VGA (640x480) | Configurable |
| **Streaming Latency** | <200ms | On local network |
| **Power Consumption** | ~160mA @ 5V | During streaming |
| **Storage Capacity** | Limited by SD card | Auto-management enabled |

### Supported Formats

- **Image Format:** JPEG
- **Video Format:** MJPEG
- **Audio:** Not supported (camera module only)

### Network Protocols

- **HTTP/1.1** for web interface
- **TCP** for reliable data transmission
- **Basic Authentication** for security

## 🔌 API Endpoints

### REST API Reference

#### Status Information
```http
GET /stats
Response: {
  "mode": "IDLE|STREAMING|RECORDING",
  "heap": 123456,
  "fps": 15.2,
  "sd_free_gb": 2.45,
  "public_ip": "203.0.113.1"
}
```

#### Control Endpoints
```http
GET /stream/start    # Start streaming mode
GET /recording/start # Start recording mode
GET /stop           # Stop all operations
GET /frame          # Get single frame (during streaming)
```

#### Authentication
All endpoints require HTTP Basic Authentication when enabled.

## 🤝 Contributing

### Development Environment Setup

1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-feature`)
3. Install development dependencies
4. Make changes and test thoroughly
5. Commit changes (`git commit -m 'Add amazing feature'`)
6. Push to branch (`git push origin feature/amazing-feature`)
7. Open Pull Request

### Code Style Guidelines

- Use consistent indentation (2 spaces)
- Comment complex logic sections
- Follow ESP32 naming conventions
- Test on actual hardware before submitting

### Reporting Issues

When reporting bugs, include:
- ESP32-CAM board version
- Arduino IDE version
- Complete error messages
- Steps to reproduce
- Network configuration details

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **Espressif Systems** - For the ESP32 platform
- **AI-Thinker** - For the ESP32-CAM module
- **Arduino Community** - For libraries and support
- **Contributors** - For improvements and bug fixes

## 📞 Support

### Getting Help

- **GitHub Issues** - For bug reports and feature requests
- **Arduino Forum** - For general ESP32 questions
- **ESP32 Documentation** - Official Espressif docs

### Quick Start Checklist

- [ ] Hardware assembled and wired correctly
- [ ] Arduino IDE configured with ESP32 support
- [ ] Required libraries installed
- [ ] Code uploaded successfully
- [ ] Network credentials configured
- [ ] Static IP assigned and accessible
- [ ] Port forwarding configured (for internet access)
- [ ] Authentication credentials changed
- [ ] System tested locally and remotely

---

**🎉 You now have a fully functional ESP32-CAM surveillance system with both local and internet access capabilities!**

---

*Last updated: September 2025*
*Version: 2.0*
