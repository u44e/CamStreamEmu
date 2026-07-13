# CamStreamEmu — reproduce a PacketScope camera profile as a live video stream.
# Video-only camera stream emulator (no PTZF control). Linux / Raspberry Pi
# (CardputerZero); dev-buildable on macOS. Needs GStreamer 1.0 (+ app,
# rtsp-server, base/good/bad/libav plugins). On a Pi it uses libcamera + v4l2 HW
# H.264; on a dev host it falls back to videotestsrc + x264enc.
CC      ?= cc
CFLAGS  ?= -std=gnu11 -Wall -Wextra -O2
GST      = gstreamer-1.0 gstreamer-app-1.0 gstreamer-rtsp-server-1.0
CFLAGS  += $(shell pkg-config --cflags $(GST))
LDLIBS  += $(shell pkg-config --libs $(GST))

SRC = cli.c profile.c video_pipe.c tts_wrap.c rtsp_serve.c mjpeg_serve.c h264_inject.c
OBJ = $(SRC:.c=.o)

camstreamemu: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f camstreamemu *.o

.PHONY: clean
