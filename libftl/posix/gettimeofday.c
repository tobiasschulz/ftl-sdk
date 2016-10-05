#include <sys/time.h>
#include <time.h>

int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

float timeval_to_ms(struct timeval *tv) {
	float sec, usec;

	sec = tv->tv_sec;
	usec = tv->tv_usec;

	return sec * 1000 + usec / 1000;
}

int Sleep(unsigned long milliseconds) {
	struct timespec req = {0};
	time_t sec = (int)(milliseconds / 1000);
	milliseconds = milliseconds - (sec * 1000);
	req.tv_sec = sec;
	req.tv_nsec = milliseconds * 1000000L;
	while (nanosleep(&req, &req) == -1) {
		continue;
	}
	return 1;
}
