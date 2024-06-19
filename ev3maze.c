#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <errno.h>
#include "ev3.h"
#include "ev3_tacho.h"
#include "SDL.h"
#include "SDL_net.h"

uint8_t sn1, sn2;
int max_speed;

int main() {
	int fd;
	struct v4l2_capability cap;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buffer;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = 4;
	void* bufmem[4];
	
	buffer.type = reqbuf.type;
	buffer.memory = reqbuf.memory;
	buffer.index = 0;
	fd = open("/dev/video0", O_RDWR);
	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
		printf("error opening video capture, couldnt find device \n");
		return -1;
	}
	printf("device name %s\n", cap.card);
	
	ioctl(fd, VIDIOC_REQBUFS, &reqbuf); // request buffers
	for (int i = 0; i < 4; i++) {
		buffer.index = i;
		buffer.type = reqbuf.type;
		ioctl(fd, VIDIOC_QUERYBUF, &buffer);
		printf("buffer %d offset = %d, lenght = %d, bytes used %d\n", i, buffer.m.offset, buffer.length, buffer.bytesused);
		bufmem[i] = mmap(NULL /* start anywhere */ ,
		   buffer.length, PROT_READ, MAP_SHARED, fd,
		   buffer.m.offset);
		ioctl(fd, VIDIOC_QBUF, &buffer); // queue reqested buffer
		if (ioctl(fd, VIDIOC_STREAMON, &reqbuf.type) == -1) {
			printf("error starting stream\n");
			return -1;
		}
		// after this, can take out buffers
	}
	
	// send image data
	
	SDLNet_init();
	IPaddress serverIP;
	TCPsocket server, client;
	
	SDLNet_ResolveHost(&serverIP, NULL, 1234);
	printf("waiting for client\n");
	server = SDLNet_TCP_Open(&serverIP);
	while (!client) {
		client = SDLNet_TCP_Accept(server);
	}
	printf("client connected\n");
	
	buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buffer.memory = V4L2_MEMORY_MMAP;
	ioctl(fd, VIDIOC_DQBUF, &buffer);
	printf("buffer index = %d bytes used %d\n", buffer.index, buffer.bytesused);
	printf("offset = %d\n", buffer.m.offset);
	for (int i = 0; i < 178*128; i++) {
		SDLNet_TCP_Send(client, (void*)(bufmem[buffer.index] + (sizeof(unsigned char) * i * 2)), sizeof(unsigned char) );
	}
	SDLNet_Quit();
	
}