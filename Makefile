CC = gcc
CFLAGS = -O3 -march=native -mtune=native -flto -funroll-loops -ffast-math -finline-functions -fomit-frame-pointer -DNDEBUG -s
TARGET = bfetch
SOURCE = fetch.c

# Ultra-aggressive optimization flags for maximum speed
AGGRESSIVE_FLAGS = -Ofast -march=native -mtune=native -flto -funroll-loops -finline-functions \
                   -ffast-math -fomit-frame-pointer -fno-stack-protector -fno-unwind-tables \
                   -fno-asynchronous-unwind-tables -DNDEBUG -s

.PHONY: all clean fast install

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

# Maximum speed build (use this one!)
fast: $(SOURCE)
	$(CC) $(AGGRESSIVE_FLAGS) -o $(TARGET) $(SOURCE)
	strip --strip-all $(TARGET)

# Install to /usr/local/bin
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

clean:
	rm -f $(TARGET)

# Build and run
run: $(TARGET)
	./$(TARGET)

# Build fast and run
fastrun: fast
	./$(TARGET)
