CXX = g++
CXXFLAGS = -fPIC -O2 $(shell pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core)
LIBS = $(shell pkg-config --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread

# Find distro-specific moc path automatically
MOC = $(shell which moc 2>/dev/null || which /usr/lib/qt6/moc 2>/dev/null || which /usr/lib/qt6/libexec/moc 2>/dev/null || which /usr/lib64/qt6/bin/moc 2>/dev/null)

TARGET = tozo_kontrol
SRC = tozo_kontrol.cpp

all: $(TARGET)

main.moc: $(SRC)
	$(MOC) $< -o $@

$(TARGET): $(SRC) main.moc
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET) main.moc
