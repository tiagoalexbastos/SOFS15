CC = gcc
CFLAGS  = -Wall 
CFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -I "../debugging" -I "../rawIO15" -I "../sofs15"

LIB_NAME = syscalls15

TARGET_LIB = lib$(LIB_NAME).a

# OBJS += soLink.o
# OBJS += soUnlink.o
# OBJS += soMknod.o
# OBJS += soRead.o
# OBJS += soWrite.o
  OBJS += soTruncate.o
# OBJS += soMkdir.o
# OBJS += soRmdir.o
# OBJS += soReaddir.o
# OBJS += soRename.o
# OBJS += soSymlink.o
# OBJS += soReadlink.o

all:			$(TARGET_LIB)

$(TARGET_LIB):		$(OBJS)
			ar -r $@ $^
			cp $@ ../../lib
			rm -f $^ $@

clean:
			rm -f $(OBJS) $(TARGET_LIB)
			rm -f ../../lib/$(TARGET_LIB)
			rm -f *~ 


