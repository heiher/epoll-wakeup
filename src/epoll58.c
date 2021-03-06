#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 *        t0    t1
 *      (p) \  / (p)
 *           e0
 *     (et) /  \ (et)
 *        e1    e2
 *    (lt) |     | (lt)
 *        s0    s2
 */

static int efd[3];
static int sfd[4];
static int count;
static pthread_t tm;
static pthread_t tw;

static void *thread_handler(void *data)
{
	struct epoll_event e;
	struct pollfd pfd;

	pfd.fd = efd[0];
	pfd.events = POLLIN;
	if (poll(&pfd, 1, -1) == 1) {
		if (epoll_wait(efd[0], &e, 1, 0) == 1)
			__sync_fetch_and_add(&count, 1);
	}

	return NULL;
}

static void signal_handler(int signum)
{
}

static void *emit_handler(void *data)
{
	usleep(100000);

	write(sfd[1], "w", 1);
	write(sfd[3], "w", 1);

	usleep(500000);
	signal(SIGUSR1, signal_handler);
	pthread_kill(tm, SIGUSR1);
	pthread_kill(tw, SIGUSR1);

	return NULL;
}

int main(int argc, char *argv[])
{
	struct epoll_event e;
	struct pollfd pfd;
	pthread_t te;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, &sfd[0]) < 0)
		goto out;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, &sfd[2]) < 0)
		goto out;

	efd[0] = epoll_create(1);
	if (efd[0] < 0)
		goto out;

	efd[1] = epoll_create(1);
	if (efd[1] < 0)
		goto out;

	efd[2] = epoll_create(1);
	if (efd[2] < 0)
		goto out;

	e.events = EPOLLIN;
	if (epoll_ctl(efd[1], EPOLL_CTL_ADD, sfd[0], &e) < 0)
		goto out;

	e.events = EPOLLIN;
	if (epoll_ctl(efd[2], EPOLL_CTL_ADD, sfd[2], &e) < 0)
		goto out;

	e.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(efd[0], EPOLL_CTL_ADD, efd[1], &e) < 0)
		goto out;

	e.events = EPOLLIN | EPOLLET;
	if (epoll_ctl(efd[0], EPOLL_CTL_ADD, efd[2], &e) < 0)
		goto out;

	if (pthread_create(&tw, NULL, thread_handler, NULL) < 0)
		goto out;

	tm = pthread_self();
	if (pthread_create(&te, NULL, emit_handler, NULL) < 0)
		goto out;

	pfd.fd = efd[0];
	pfd.events = POLLIN;
	if (poll(&pfd, 1, -1) == 1) {
		if (epoll_wait(efd[0], &e, 1, 0) == 1)
			__sync_fetch_and_add(&count, 1);
	}

	if (pthread_join(tw, NULL) < 0)
		goto out;

	if (count != 2)
		goto out;

	close(efd[0]);
	close(efd[1]);
	close(efd[2]);
	close(sfd[0]);
	close(sfd[1]);
	close(sfd[2]);
	close(sfd[3]);

	return 0;

out:
	return -1;
}
