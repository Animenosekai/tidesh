#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif
#include "commands/myinfo.h"

int builtin_myinfo(int argc, char **argv, Session *session) {
    (void)argc;
    (void)argv;
    (void)session; // Evite les warnings

    // LIGNE 1 : <hostname> <kernel-version> <arch>
    char           hostname[256] = "unknown";
    char           kernel[256]   = "unknown";
    struct utsname uts;
    uname(&uts);

#if defined(__APPLE__)
    size_t len = sizeof(hostname);
    sysctlbyname("kern.hostname", hostname, &len, NULL, 0);
    // Use uts.release (e.g. 23.0.0) or fetch kern.osrelease
    strncpy(kernel, uts.release, sizeof(kernel) - 1);
#else
    // Hostname (/proc/sys/kernel/hostname)
    FILE *f_host = fopen("/proc/sys/kernel/hostname", "r");
    if (f_host) {
        fscanf(f_host, "%255s", hostname);
        fclose(f_host);
    }

    //  Kernel Version (/proc/version)
    FILE *f_ver = fopen("/proc/version", "r");
    if (f_ver) { // On récupère juste le 3ème mot (souvent la version du noyau)
        char junk[256];
        fscanf(f_ver, "%s %s %255s", junk, junk, kernel);
        fclose(f_ver);
    }
#endif

    //  Processor Architecture arch
    printf("%s %s %s\n", hostname, kernel, uts.machine);

    // LIGNE 2 : <time> up <nb> days, <hh>:<mm>
    // Obtenir l'heure actuelle
    time_t     now        = time(NULL);
    struct tm *t          = localtime(&now);
    double     uptime_sec = 0;

#if defined(__APPLE__)
    struct timeval boottime;
    size_t         len_bt = sizeof(boottime);
    if (sysctlbyname("kern.boottime", &boottime, &len_bt, NULL, 0) == 0) {
        uptime_sec = difftime(now, boottime.tv_sec);
    }
#else
    // Lecture de /proc/uptime
    FILE *f_up = fopen("/proc/uptime", "r");
    if (f_up) {
        fscanf(f_up, "%lf", &uptime_sec);
        fclose(f_up);
    }
#endif
    // Conversion des secondes en jours, heures et minutes
    long d = (long)uptime_sec / 86400;
    long h = ((long)uptime_sec % 86400) / 3600;
    long m = ((long)uptime_sec % 3600) / 60;

    // Afficher l'heure au format HH:MM:SS
    printf("%02d:%02d:%02d up %ld days, %02ld:%02ld\n", t->tm_hour, t->tm_min,
           t->tm_sec, d, h, m);

    // LIGNE 3 : Load : <1-min> - <5-min> - <15-min>
    double l1, l5, l15;
#if defined(__APPLE__)
    double loads[3];
    if (getloadavg(loads, 3) != -1) {
        l1  = loads[0];
        l5  = loads[1];
        l15 = loads[2];
        printf("Load : %.2f - %.2f - %.2f\n", l1, l5, l15);
    }
#else
    FILE *f_load = fopen("/proc/loadavg", "r");
    if (f_load) {
        fscanf(f_load, "%lf %lf %lf", &l1, &l5, &l15);
        printf("Load : %.2f - %.2f - %.2f\n", l1, l5, l15);
        fclose(f_load);
    }
#endif

    return 0;
}