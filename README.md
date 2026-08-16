# TOZO HT3 Kontrol Panel (Qt / C++ Linux)

A native Linux desktop application for controlling TOZO Bluetooth headphones (ANC modes, low latency, spatial audio, and a 10-band hardware equalizer with custom preset persistence).

---

## 1. Prerequisites & Dependencies

Install the required build tools, Qt development libraries, and BlueZ development headers for your distribution:

### Arch Linux / Manjaro
```bash
sudo pacman -S base-devel gcc pkgconf qt6-base bluez-libs
```

### Debian / Ubuntu / Linux Mint
```bash
# Qt6 (Recommended)
sudo apt update
sudo apt install build-essential g++ pkg-config qt6-base-dev qt6-base-dev-tools libbluetooth-dev

# Qt5 (Alternative)
sudo apt install build-essential g++ pkg-config qtbase5-dev qtbase5-dev-tools libbluetooth-dev
```

### Fedora
```bash
sudo dnf install gcc-c++ pkgconf-pkg-config qt6-qtbase-devel bluez-libs-devel
```

---

## 2. Directory Structure

Place your source files and application icon in the same directory:

```text
tozo-kontrol/
├── main.cpp
├── icon.png        # (or icon.jpg / icon.svg)
└── Makefile        # (optional)
```

---

## 3. Building the Application

### Option A: Direct Command Line (Distro-Specific `moc` Paths)

#### Arch Linux (Qt6)
```bash
/usr/lib/qt6/moc main.cpp -o main.moc && g++ -fPIC main.cpp -o tozo_kontrol $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread
```

#### Debian / Ubuntu / Linux Mint (Qt6)
```bash
/usr/lib/qt6/libexec/moc main.cpp -o main.moc && g++ -fPIC main.cpp -o tozo_kontrol $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread
```

#### Fedora (Qt6)
```bash
/usr/lib64/qt6/bin/moc main.cpp -o main.moc && g++ -fPIC main.cpp -o tozo_kontrol $(pkg-config --cflags --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread
```

#### Generic Qt5
```bash
moc main.cpp -o main.moc && g++ -fPIC main.cpp -o tozo_kontrol $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core) -lbluetooth -lpthread
```

---

### Option B: Using a Makefile (Auto-detects `moc`)

Save this `Makefile` in the project root:

```makefile
CXX = g++
CXXFLAGS = -fPIC -O2 $(shell pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core)
LIBS = $(shell pkg-config --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread

# Find distro-specific moc path automatically
MOC = $(shell which moc 2>/dev/null || which /usr/lib/qt6/moc 2>/dev/null || which /usr/lib/qt6/libexec/moc 2>/dev/null || which /usr/lib64/qt6/bin/moc 2>/dev/null)

TARGET = tozo_kontrol
SRC = main.cpp

all: $(TARGET)

main.moc: $(SRC)
	$(MOC) $< -o $@

$(TARGET): $(SRC) main.moc
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) main.moc
```

Compile with:
```bash
make
```

---

## 4. Desktop Launcher & Icon Integration (KDE / GNOME / Wayland)

To show the app with its icon in your desktop launcher and taskbar/dock:

1. **Install the Icon:**
   ```bash
   mkdir -p ~/.local/share/icons/hicolor/256x256/apps/
   cp icon.png ~/.local/share/icons/hicolor/256x256/apps/tozo-kontrol.png
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

All presets and configuration states are automatically written to `~/.config/tozo_config.json`.
