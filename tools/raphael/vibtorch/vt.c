/* vt: the raphael sysctls of patches 0263 and 0265 from a shell on the phone.
 * vt state | vt vib MS | vt torch 0..1000 | vt bl 0..1000 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>

int
main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "state")) {
		char line[512] = "";
		size_t len = sizeof(line) - 1;
		int v = -1, t = -1;
		size_t l = sizeof(v);
		if (sysctlbyname("kern.sweet_vibtorch_state", line, &len, NULL, 0) != 0) {
			printf("kern.sweet_vibtorch_state: errno %d (%s)\n", errno, strerror(errno));
		} else {
			printf("%s\n", line);
		}
		sysctlbyname("kern.sweet_vibrate", &v, &l, NULL, 0);
		l = sizeof(t);
		sysctlbyname("kern.sweet_torch", &t, &l, NULL, 0);
		printf("kern.sweet_vibrate=%d kern.sweet_torch=%d\n", v, t);
		l = sizeof(v);
		if (sysctlbyname("kern.sweet_brightness", &v, &l, NULL, 0) == 0) {
			printf("kern.sweet_brightness=%d\n", v);
		}
		return 0;
	}
	if (argc == 3 && (!strcmp(argv[1], "vib") || !strcmp(argv[1], "torch") || !strcmp(argv[1], "bl"))) {
		const char *name = argv[1][0] == 'v' ? "kern.sweet_vibrate" : argv[1][0] == 't' ? "kern.sweet_torch" :
		    "kern.sweet_brightness";
		int value = atoi(argv[2]);
		if (sysctlbyname(name, NULL, NULL, &value, sizeof(value)) != 0) {
			printf("%s=%d: errno %d (%s)\n", name, value, errno, strerror(errno));
			return 1;
		}
		printf("%s=%d: ok\n", name, value);
		return 0;
	}
	fprintf(stderr, "usage: vt state | vt vib MS | vt torch 0..1000 | vt bl 0..1000\n");
	return 2;
}
