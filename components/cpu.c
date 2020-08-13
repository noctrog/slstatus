/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../util.h"

#if defined(__linux__)
	const char *
	cpu_freq(void)
	{
		uintmax_t freq;

		/* in kHz */
		if (pscanf("/sys/devices/system/cpu/cpu0/cpufreq/"
		           "scaling_cur_freq", "%ju", &freq) != 1) {
			return NULL;
		}

		return fmt_human(freq * 1000, 1000);
	}

    #ifndef N_CPUS
	const char *
	cpu_perc(void)
	{
		static long double a[7];
		long double b[7], sum;

		memcpy(b, a, sizeof(b));
		/* cpu user nice system idle iowait irq softirq */
		if (pscanf("/proc/stat", "%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf",
		           &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6])
		    != 7) {
			return NULL;
		}
		if (b[0] == 0) {
			return NULL;
		}
        /*warn("Blabla: %Lf %Lf %Lf %Lf %Lf %Lf %Lf",*/
                /*&a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6]);*/

		sum = (b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6]) -
		      (a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6]);

		if (sum == 0) {
			return NULL;
		}

		return bprintf("%d", (int)(100 *
		               ((b[0] + b[1] + b[2] + b[5] + b[6]) -
		                (a[0] + a[1] + a[2] + a[5] + a[6])) / sum));
	}
    #else

/* colors */
#define BLUE 

	const char *
	cpu_perc(void)
	{
		static long double a[N_CPUS][7];
		long double b[N_CPUS][7], sum[N_CPUS];

		memcpy(*b, *a, N_CPUS * 7 * sizeof(long double));
		/* cpu user nice system idle iowait irq softirq */
        FILE *fp;
        if (!(fp = fopen("/proc/stat", "r"))) {
            warn("fopen '%s':", "/proc/stat");
            return NULL;
        }

        char *line;
        ssize_t linelen = 0;
        size_t linecap = 0;
        int j = 0;
        while ((linelen = getline(&line, &linecap, fp)) != -1 && j <= N_CPUS) {
            /* First line is overall CPU */
            if (j == 0) {
                ++j;
                continue;
            }

            /*warn("%s", line);*/
            
            /*char *token = strtok(line, " \t");*/
            /*int i = 0;*/
            /*while (i <= 7 && token != NULL) {*/
                /*if (i > 0) {*/
                    /*a[j-1][i-1] = strtod(token, NULL);*/
                    /*warn("%Lf", a[j-1][i-1]);*/
                /*}*/

                /*token = strtok(NULL, " \t");*/
                
                /*++i;*/
            /*}*/
            if (sscanf(line, "%*s %Lf %Lf %Lf %Lf %Lf %Lf %Lf",
                    &a[j-1][0], &a[j-1][1], &a[j-1][2], &a[j-1][3], &a[j-1][4], &a[j-1][5], &a[j-1][6])
                    != 7) {
                warn("sscanf error");
                return NULL;
            }

            ++j;
        }

        free(line);
        fclose(fp);

        if (b[0][0] == 0) return NULL;

        for (j = 0; j < N_CPUS; ++j) {
            sum[j] = (b[j][0] + b[j][1] + b[j][2] + b[j][3] + b[j][4] + b[j][5] + b[j][6]) -
                     (a[j][0] + a[j][1] + a[j][2] + a[j][3] + a[j][4] + a[j][5] + a[j][6]);
            if (sum[j] == 0) return NULL;
        }

        /* Draw background rectangles */
        const char *bar_foreground = "^c#cc241d^";
        const char *bar_background = "^c#282828^";
        const char *background = "^c#458588^";
        const int bar_width = 6;
        const int bar_interspace = 2;
        const int bar_top = 2;
        const int bar_bottom = 18;

        char buf[512];
        ssize_t idx = 0;
        /*Draw background*/
        idx += snprintf(&buf[idx], strlen(background) + 15 + 1, "%s^r%02d,%02d,%02d,%02d^", 
                background, 0, 0, (bar_width + bar_interspace) * N_CPUS, 20);
        /*Draw bar background*/
        idx += snprintf(&buf[idx], strlen(bar_background) + 1, "%s", bar_background);
        for (j = 0; j < N_CPUS; ++j) {      /* background */
            idx += snprintf(&buf[idx], sizeof(buf) - idx, "^r%d,%d,%d,%d^", 
                            j*(bar_width + bar_interspace), bar_top, bar_width, bar_bottom - bar_top);
        }
        /*Draw percentage bars*/
        idx += snprintf(&buf[idx], strlen(bar_foreground) + 1, "%s", bar_foreground);
        for (j = 0; j < N_CPUS; ++j) {      /* foreground */
            int perc = (int)((bar_bottom - bar_top) *
                       ((b[j][0] + b[j][1] + b[j][2] + b[j][5] + b[j][6]) -
                        (a[j][0] + a[j][1] + a[j][2] + a[j][5] + a[j][6])) / sum[j]);
            idx += snprintf(&buf[idx], sizeof(buf) - idx, "^r%d,%d,%d,%d^", 
                            j*(bar_width + bar_interspace), bar_bottom - perc,
                            bar_width, perc);
        }
        idx += snprintf(&buf[idx], 7, "^f%03d^", j * 8);

        return bprintf("%s", buf);
	}
    #endif 
#elif defined(__OpenBSD__)
	#include <sys/param.h>
	#include <sys/sched.h>
	#include <sys/sysctl.h>

	const char *
	cpu_freq(void)
	{
		int freq, mib[2];
		size_t size;

		mib[0] = CTL_HW;
		mib[1] = HW_CPUSPEED;

		size = sizeof(freq);

		/* in MHz */
		if (sysctl(mib, 2, &freq, &size, NULL, 0) < 0) {
			warn("sysctl 'HW_CPUSPEED':");
			return NULL;
		}

		return fmt_human(freq * 1E6, 1000);
	}

	const char *
	cpu_perc(void)
	{
		int mib[2];
		static uintmax_t a[CPUSTATES];
		uintmax_t b[CPUSTATES], sum;
		size_t size;

		mib[0] = CTL_KERN;
		mib[1] = KERN_CPTIME;

		size = sizeof(a);

		memcpy(b, a, sizeof(b));
		if (sysctl(mib, 2, &a, &size, NULL, 0) < 0) {
			warn("sysctl 'KERN_CPTIME':");
			return NULL;
		}
		if (b[0] == 0) {
			return NULL;
		}

		sum = (a[CP_USER] + a[CP_NICE] + a[CP_SYS] + a[CP_INTR] + a[CP_IDLE]) -
		      (b[CP_USER] + b[CP_NICE] + b[CP_SYS] + b[CP_INTR] + b[CP_IDLE]);

		if (sum == 0) {
			return NULL;
		}

		return bprintf("%d", 100 *
		               ((a[CP_USER] + a[CP_NICE] + a[CP_SYS] +
		                 a[CP_INTR]) -
		                (b[CP_USER] + b[CP_NICE] + b[CP_SYS] +
		                 b[CP_INTR])) / sum);
	}
#elif defined(__FreeBSD__)
	#include <sys/param.h>
	#include <sys/sysctl.h>
	#include <devstat.h>

	const char *
	cpu_freq(void)
	{
		int freq;
		size_t size;

		size = sizeof(freq);
		/* in MHz */
		if (sysctlbyname("hw.clockrate", &freq, &size, NULL, 0) == -1
				|| !size) {
			warn("sysctlbyname 'hw.clockrate':");
			return NULL;
		}

		return fmt_human(freq * 1E6, 1000);
	}

	const char *
	cpu_perc(void)
	{
		size_t size;
		static long a[CPUSTATES];
		long b[CPUSTATES], sum;

		size = sizeof(a);
		memcpy(b, a, sizeof(b));
		if (sysctlbyname("kern.cp_time", &a, &size, NULL, 0) == -1
				|| !size) {
			warn("sysctlbyname 'kern.cp_time':");
			return NULL;
		}
		if (b[0] == 0) {
			return NULL;
		}

		sum = (a[CP_USER] + a[CP_NICE] + a[CP_SYS] + a[CP_INTR] + a[CP_IDLE]) -
		      (b[CP_USER] + b[CP_NICE] + b[CP_SYS] + b[CP_INTR] + b[CP_IDLE]);

		if (sum == 0) {
			return NULL;
		}

		return bprintf("%d", 100 *
		               ((a[CP_USER] + a[CP_NICE] + a[CP_SYS] +
		                 a[CP_INTR]) -
		                (b[CP_USER] + b[CP_NICE] + b[CP_SYS] +
		                 b[CP_INTR])) / sum);
	}
#endif
