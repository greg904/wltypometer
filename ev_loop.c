#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-client.h>

int ev_loop_timer = -1;

void (*ev_loop_timer_cb)() = NULL;

int ev_loop_error = 0;

static struct timespec prev_poll;

static int go(struct wl_display *wl_display)
{
	struct pollfd poll_fds[1];
	int ret;
	struct timespec now;

	if (ev_loop_timer == 0) {
		ev_loop_timer = -1;

		assert(ev_loop_timer_cb);
		ev_loop_timer_cb();
	}

	while (wl_display_prepare_read(wl_display) == -1) {
		if (wl_display_dispatch_pending(wl_display) == -1) {
			perror("wl_display_dispatch_pending");
			return -1;
		}
	}

	poll_fds[0].fd = wl_display_get_fd(wl_display);
	poll_fds[0].events = POLLIN;

	if (wl_display_flush(wl_display) == -1) {
		if (errno != EAGAIN) {
			perror("wl_display_flush");
			return -1;
		}

		poll_fds[0].events |= POLLOUT;
	}

	ret = poll(poll_fds, sizeof(poll_fds) / sizeof(*poll_fds), ev_loop_timer);
	if (ret == -1) {
		perror("poll");
		return -1;
	}

	if ((poll_fds[0].revents & POLLERR) != 0) {
		fputs("Error on Wayland FD.\n", stderr);
		return -1;
	}

	if ((poll_fds[0].revents & POLLIN) != 0) {
		if (wl_display_read_events(wl_display) == -1) {
			perror("wl_display_read_events");
			return -1;
		}
	} else {
		wl_display_cancel_read(wl_display);
	}

	if ((poll_fds[0].revents & POLLOUT) != 0 && wl_display_flush(wl_display) == -1) {
		perror("wl_display_flush");
		return -1;
	}

	clock_gettime(CLOCK_MONOTONIC, &now);

	if (ret == 0) {
		ev_loop_timer = 0;
	} else if (ev_loop_timer > 0) {
		ev_loop_timer -=
		    now.tv_nsec - prev_poll.tv_nsec + (now.tv_sec - prev_poll.tv_sec) * 1000000000;
		ev_loop_timer = ev_loop_timer < 0 ? 0 : ev_loop_timer;
	}

	prev_poll = now;

	return 0;
}

int ev_loop_run(struct wl_display *wl_display)
{
	clock_gettime(CLOCK_MONOTONIC, &prev_poll);

	while (!ev_loop_error && go(wl_display) != -1) {
	}

	/* We only get here when there is an error. */
	return -1;
}
