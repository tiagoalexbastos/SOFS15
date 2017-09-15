CC = gcc
CFLAGS  = -Wall 
CFLAGS += -D_FILE_OFFSET_BITS=64 
CFLAGS += -DFUSE_USE_VERSION=26 
CFLAGS += -I "../debugging" -I "../rawIO15" 
CFLAGS += -I "../sofs15" -I "../syscalls15"

TARGET = mount_sofs15

SUFFIX = $(shell getconf LONG_BIT)

LIBS =
LIBS += -lsyscalls15
LIBS += -lsyscalls15bin_$(SUFFIX)
LIBS += -lsofs15
LIBS += -lsofs15bin_$(SUFFIX)
LIBS += -lrawIO15bin_$(SUFFIX)
LIBS += -lrawIO15
LIBS += -ldebugging
LIBS += -lpthread
LIBS += -lfuse

LFLAGS = -L "../../lib" $(LIBS)

OBJS = 

all:		$(TARGET)

$(TARGET):	$(TARGET).o $(OBJS)
			$(CC) -o $@ $^ $(LFLAGS)
			cp $@ ../../run
			rm -f $^ $@

clean:
			rm -f $(TARGET) $(TARGET).o $(OBJS)
			rm -f ../../run/$(TARGET)
			rm -f *~ 
