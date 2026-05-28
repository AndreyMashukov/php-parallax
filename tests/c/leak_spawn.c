/* RSS-drift gate. Spawns N tasks, measures resident memory before/after,
 * fails if the drift exceeds a fixed threshold. */

#include "value.h"
#include "waitgroup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static long rss_kb(void)
{
	FILE *f = fopen("/proc/self/status", "r");
	if (f == NULL) {
		return -1;
	}
	char line[256];
	long kb = -1;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strncmp(line, "VmRSS:", 6) == 0) {
			sscanf(line + 6, "%ld", &kb);
			break;
		}
	}
	fclose(f);
	return kb;
}

static void payload_runner(task_t *task)
{
	int64_t in = task->args->as.i;
	value_t *out = value_arr(0);
	for (int j = 0; j < 4; j++) {
		value_arr_push(out, value_long(in + j));
	}
	task->ok = true;
	task->result = out;
}

int main(int argc, char **argv)
{
	int total = (argc >= 2) ? atoi(argv[1]) : 10000;
	int batch = 200;

	long rss_baseline = rss_kb();
	if (rss_baseline < 0) {
		fprintf(stderr, "leak: /proc/self/status unavailable, skipping\n");
		return 0;
	}

	int batches = (total + batch - 1) / batch;
	for (int b = 0; b < batches; b++) {
		wait_group_t *wg = wg_create(payload_runner);
		int this_batch = (b == batches - 1) ? (total - b * batch) : batch;
		for (int i = 0; i < this_batch; i++) {
			wg_go(wg, value_long(b * batch + i), NULL);
		}
		wg_wait(wg);
		wg_destroy(wg);
	}

	long rss_after = rss_kb();
	long drift_kb = rss_after - rss_baseline;
	fprintf(stderr, "leak: baseline=%ld kB  after=%ld kB  drift=%ld kB  (%d spawns)\n",
		rss_baseline, rss_after, drift_kb, total);

	const long THRESHOLD_KB = 5 * 1024; /* 5 MB */
	if (drift_kb > THRESHOLD_KB) {
		fprintf(stderr, "leak: drift %ld kB > threshold %ld kB\n", drift_kb, THRESHOLD_KB);
		return 1;
	}
	return 0;
}
