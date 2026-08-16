CXX = g++
CXXFLAGS = -fPIC -Os -s $(shell pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core)
LIBS = $(shell pkg-config --libs Qt6Widgets Qt6Gui Qt6Core) -lbluetooth -lpthread

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
