# use pkg-config for getting CFLAGS and LDLIBS
PROG_NAME := cruncher

FFMPEG_LIBS=    libavdevice                        \
                libavformat                        \
                libavfilter                        \
                libavcodec                         \
                libswresample                      \
                libswscale                         \
                libavutil                          \
                jansson                            \

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDLIBS := -Wl,--no-as-needed $(shell pkg-config --libs $(FFMPEG_LIBS)) -lm $(LDLIBS)
endif
    ifeq ($(UNAME_S),Darwin)
    LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) -lm $(LDLIBS)
endif

COPTS := -Wall -Wextra -std=c99
CFLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS))

.phony: all clean

default: prod

clean:
	$(RM) *.o *.mpg *.mp4

debugall: clean
debugall: COPTS += -DDEBUG_WRITE -DDEBUG_FRAME -DDEBUG_FMT
debugall: debug

debugfmt: clean
debugfmt: COPTS += -DDEBUG_FMT
debugfmt: debug

debugframe: clean
debugframe: COPTS += -DDEBUG_FRAME
debugframe: debug

debugwrite: clean
debugwrite: COPTS += -DDEBUG_WRITE
debugwrite: debug

debug: clean
debug: COPTS += -g -O0
debug: CFLAGS += $(COPTS)
debug: executable

prod: COPTS += -O2
prod: CFLAGS += $(COPTS)
prod: executable

# $@ = target
# $^ = dependencies
executable: main.o video.o json.o utils.o actualizer.o
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $(PROG_NAME)

# $< = first dependency
main.o: main.c video.h json.h utils.h
	$(CC) $(CFLAGS) -c $<

video.o: video.c video.h
	$(CC) $(CFLAGS) -c $<

json.o: json.c json.h
	$(CC) $(CFLAGS) -c $<

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c $<

actualizer.o: actualizer.c actualizer.h
	$(CC) $(CFLAGS) -c $<
