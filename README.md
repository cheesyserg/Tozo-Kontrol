# TOZO HT3 Kontrol Panel (Qt / C++ Linux)

A native Linux desktop application for controlling TOZO Bluetooth headphones (ANC modes, low latency, spatial audio, and a 10-band hardware equalizer with custom preset persistence).

---

## 1. Prerequisites & Dependencies

Install the build tools, Qt development libraries, BlueZ development headers, and `xxd` (for embedding the application icon)[cite: 2]:

### Arch Linux / Manjaro
```bash
sudo pacman -S base-devel gcc pkgconf qt6-base bluez-libs xxd
```

### Debian / Ubuntu / Linux Mint
```bash
# Qt6 (Recommended)
sudo apt update
sudo apt install build-essential g++ pkg-config qt6-base-dev qt6-base-dev-tools libbluetooth-dev xxd

# Qt5 (Alternative)
sudo apt install build-essential g++ pkg-config qtbase5-dev qtbase5-dev-tools libbluetooth-dev xxd
```

### Fedora
```bash
sudo dnf install gcc-c++ pkgconf-pkg-config qt6-qtbase-devel bluez-libs-devel xxd
```

---

## 2. Directory Structure

Place your source files and icon in the same directory:

```text
tozo-kontrol/
├── tozo_kontrol.cpp
├── icon.jpg
└── Makefile        # (optional)
```

---

## 3. Building the Application

### Option A: Using the Makefile (Recommended)

The Makefile automatically generates the embedded icon header (`icon_data.h`), runs `moc`, and builds a size-optimized, stripped binary.

Save this `Makefile` in the project root:

```makefile
CXX = g++
CXXFLAGS = -fPIC -Os -s $(shell pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core)
LIBS = $(shell pkg-config --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread

# Find distro-specific moc path automatically
MOC = $(shell which moc 2>/dev/null || which /usr/lib/qt6/moc 2>/dev/null || which /usr/lib/qt6/libexec/moc 2>/dev/null || which /usr/lib64/qt6/bin/moc 2>/dev/null)

TARGET = tozo_kontrol
SRC = tozo_kontrol.cpp

.PHONY: all clean

all: $(TARGET)

icon_data.h: icon.jpg
	xxd -i $< > $@

main.moc: $(SRC)
	$(MOC) $< -o $@

$(TARGET): $(SRC) main.moc icon_data.h
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) main.moc icon_data.h
```

Compile by running:
```bash
make
```

---

### Option B: Direct Command Line

First, generate the embedded icon header:
```bash
xxd -i icon.jpg > icon_data.h
```

Then compile using your distribution's `moc` path:

#### Arch Linux (Qt6)
```bash
/usr/lib/qt6/moc tozo_kontrol.cpp -o main.moc && g++ -fPIC -Os -s tozo_kontrol.cpp -o tozo_kontrol $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread
```

#### Debian / Ubuntu / Linux Mint (Qt6)
```bash
/usr/lib/qt6/libexec/moc tozo_kontrol.cpp -o main.moc && g++ -fPIC -Os -s tozo_kontrol.cpp -o tozo_kontrol $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread
```

#### Fedora (Qt6)
```bash
/usr/lib64/qt6/bin/moc tozo_kontrol.cpp -o main.moc && g++ -fPIC -Os -s tozo_kontrol.cpp -o tozo_kontrol $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread
```

---

## 4. Desktop Launcher & System Menu Integration

The application embeds its icon directly into the binary at runtime. To also display the icon in your desktop app menu:

1. **Install the Icon Asset:**
   ```bash
   mkdir -p ~/.local/share/icons/hicolor/256x256/apps/
   cp icon.jpg ~/.local/share/icons/hicolor/256x256/apps/tozo-kontrol.jpg
   ```

2. **Create Desktop Entry:**
   Create `~/.local/share/applications/tozo-kontrol.desktop`:
   ```ini
   [Desktop Entry]
   Name=TOZO HT3 Kontrol
   Comment=TOZO Headphone Control & Equalizer Panel
   Exec=/absolute/path/to/tozo_kontrol
   Icon=tozo-kontrol
   Terminal=false
   Type=Application
   StartupWMClass=tozo-kontrol
   Categories=AudioVideo;Audio;Settings;
   ```

3. **Update Desktop Caches:**
   ```bash
   update-desktop-database ~/.local/share/applications
   gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor
   ```

---

## 5. Usage

1. Pair your TOZO headphones via your system's Bluetooth settings or `bluetoothctl`.
2. Run the application:
   ```bash
   ./tozo_kontrol
   ```
3. Click **Scan** to detect paired devices.
4. Select your device from the dropdown and click **Connect**.

Presets and custom configurations are automatically saved to `~/.config/tozo_config.json`.
