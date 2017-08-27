OUT?=ddb_fade.so

CFLAGS+=-std=c99 -fPIC -Wall -shared -lm -I../include -O3

SOURCES=fade.c

OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(OUT)

$(OUT): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(BS2B_LIBS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -f $(OBJECTS) $(OUT)
