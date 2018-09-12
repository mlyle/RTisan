#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>

#include <rtisan.h>
#include <rtisan_assert.h>
#include <rtisan_stream.h>

struct RTTCPStream {
	RTStream_t stream;
	int listenSock;
	int activeSock;

	RTTask_t task;
};

static void TCPStreamTask(void *ctx)
{
	struct RTTCPStream *tcp = ctx;

	while (1) {
		while (tcp->activeSock <= 0) {
			tcp->activeSock = accept(tcp->listenSock, NULL,
					NULL);
		}

		int on = 1;

		ioctl(tcp->activeSock, FIONBIO, &on);

		/* XXX select etc */

		bool didSomething = false;

		int avail;

		char *rPos = RTStreamZeroCopyRXPos(tcp->stream,
				&avail);

		if (avail) {
			int cnt = read(tcp->activeSock, rPos, avail);

			if (cnt < 0) {
				assert((errno == EAGAIN) || (errno = EINTR));
			} else if (cnt == 0) {
				close(tcp->activeSock);
				tcp->activeSock = -1;

				continue;
			} else {
				didSomething = true;
				RTStreamZeroCopyRXDone(tcp->stream, cnt);
			}
		}

		const char *wPos = RTStreamZeroCopyTXPos(tcp->stream,
				&avail);

		if (avail) {
			int cnt = write(tcp->activeSock, wPos, avail);

			if (cnt > 0) {
				didSomething = true;
				RTStreamZeroCopyTXDone(tcp->stream, cnt);
			}
		}

		if (didSomething) {
			RTSleep(1);
		}
	}
}

int RTTCPAttach(RTStream_t stream, int portNum)
{
	struct RTTCPStream *tcp = calloc(1, sizeof(*tcp));

	signal(SIGPIPE, SIG_IGN);

	tcp->stream = stream;

	tcp->listenSock = socket(AF_INET, SOCK_STREAM, 0);

	if (tcp->listenSock < 0) {
		perror("socket");
		return -1;
	}

	int on = 1;

	setsockopt(tcp->listenSock, SOL_SOCKET, SO_REUSEADDR,
			   &on, sizeof(on));

	struct sockaddr_in servAddr = {
		.sin_family = AF_INET,
		.sin_addr = { .s_addr = INADDR_ANY },
		.sin_port = htons(portNum)
	};

	if (bind(tcp->listenSock, (struct sockaddr *) &servAddr,
				sizeof(servAddr)) < 0) {
		perror("bind");
		close(tcp->listenSock);
		tcp->listenSock = -1;
		return -1;
	}

	listen(tcp->listenSock, 1);

	tcp->task = RTTaskCreate(40, TCPStreamTask, tcp);

	assert(tcp->task);

	return 0;
}

