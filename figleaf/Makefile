
JPEG_SOURCES=jpeg-6b/*.c
JPEG_HEADERS=jpeg-6b/*.h

COMMON_HEADERS=tpe.h common.h jutil.h fpe.h fisheryates.h
COMMON_OBJS=common.o jutil.o util.o random.o fisheryates.o fpe.o tpe.o shuffle.o cascade.o bounce.o gibbs.o noop.o minmax.o lsb.o mosaic.o kdf.o drpe.o drpe_lsb.o

CFLAGS=-g -I. -I./jpeg-6b/ -Wall -std=c99
ifeq ($(CC),gcc)
  CFLAGS += -Og
endif
LDFLAGS=-lm -lsodium

all: libjpeg.a figleaf
#all: tests

tests: testfpe testgibbs

libjpeg.a: $(JPEG_SOURCES) $(JPEG_HEADERS)
	$(MAKE) -C jpeg-6b
	cp jpeg-6b/libjpeg.a .

figleaf: figleaf.o libjpeg.a $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o figleaf figleaf.o libjpeg.a $(COMMON_OBJS) $(LDFLAGS)

#figleaf.o: figleaf.c $(COMMON_HEADERS)
#	$(CC) $(CFLAGS) -c figleaf.c

jutil.o: jutil.c jutil.h
	$(CC) $(CFLAGS) -c jutil.c

#testfpe: testfpe.o fpe.o
#	$(CC) $(CFLAGS) -o testfpe testfpe.o fpe.o $(LDFLAGS)
#
#testgibbs: testgibbs.o gibbs.o util.o random.o fisheryates.o jutil.o libjpeg.a
#	$(CC) $(CFLAGS) -o testgibbs testgibbs.o gibbs.o util.o random.o fisheryates.o jutil.o libjpeg.a $(LDFLAGS)

%.o: %.c $(COMMON_HEADERS)
	$(CC) -c $(CFLAGS) $< -o $@


clean:
	rm -f libjpeg.a *.o figleaf testfpe
