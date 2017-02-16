OBJS:=serverside.o main.o
NVIDIA_DRIVER_VERSION=$(shell cat /proc/driver/nvidia/version | sed -e 2d | sed -E 's,.* ([0-9]*)\.([0-9]*) .*,\1,')
LDFLAGS+= -L/usr/lib/nvidia-$(NVIDIA_DRIVER_VERSION)/ -lnvidia-ml -lxml2
CFLAGS:= -g -I/usr/include/libxml2
TARGET:=aml

all: $(OBJS)
	gcc $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o:%.c
	gcc $(CFLAGS) -c $^

clean:
	rm -f *.o
