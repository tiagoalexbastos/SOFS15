CC = gcc
CFLAGS = -Wall -I "../debugging" -I "../rawIO15" -I "../sofs15"

TARGET = mkfs_sofs15

SUFFIX = $(shell getconf LONG_BIT)

LIBS  = -lsofs15bin_$(SUFFIX)
LIBS += -lsofs15
LIBS += -lrawIO15bin_$(SUFFIX)
LIBS += -lrawIO15
LIBS += -ldebugging

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

