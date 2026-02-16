#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__APPLE__)
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#include "commands/mymaps.h"

int builtin_mymaps(int argc, char **argv, Session *session) {
    (void)session;

    // Vérification des arguments : on attend "-p <PID>"
    if (argc < 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: mymaps -p <PID>\n");
        return 1;
    }

#if defined(__APPLE__)
    int           pid = atoi(argv[2]);
    task_t        task;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        fprintf(
            stderr,
            "mymaps: failed to get task for pid %d (error %d). need sudo?\n",
            pid, kr);
        return 1;
    }

    mach_vm_address_t              address = 0;
    mach_vm_size_t                 size    = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t         count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t                    object_name;

    printf("%-20s %-5s %-10s %-10s %s\n", "ADRESSE", "PERMS", "OFFSET", "INODE",
           "PATH");

    while (1) {
        count = VM_REGION_BASIC_INFO_COUNT_64;
        kr    = mach_vm_region(task, &address, &size, VM_REGION_BASIC_INFO_64,
                               (vm_region_info_t)&info, &count, &object_name);
        if (kr != KERN_SUCCESS) {
            break;
        }

        char perms[5] = "---";
        if (info.protection & VM_PROT_READ)
            perms[0] = 'r';
        if (info.protection & VM_PROT_WRITE)
            perms[1] = 'w';
        if (info.protection & VM_PROT_EXECUTE)
            perms[2] = 'x';
        perms[3] = (info.shared) ? 's' : 'p';
        perms[4] = '\0';

        char path[PROC_PIDPATHINFO_MAXSIZE] = "";
        proc_regionfilename(pid, address, path, sizeof(path));

        printf("%012llx-%012llx %s %08llx %ld %s\n", address, address + size,
               perms, info.offset, 0L, path);

        if (perms[1] == 'w' && perms[2] == 'x') {
            printf("[*] suspicious\n");
        }

        address += size;
    }

    return 0;

#else
    char path[256];
    snprintf(path, sizeof(path), "/proc/%s/maps", argv[2]);

    FILE *f = fopen(path, "r");
    if (!f) {
        perror("mymaps");
        return 1;
    }

    char line[1024];
    printf("%-20s %-5s %-10s %-10s %s\n", "ADRESSE", "PERMS", "OFFSET", "INODE",
           "PATH");

    while (fgets(line, sizeof(line), f)) {
        unsigned long long start, end, offset;
        char               perms[5];
        long               inode;
        char               pathname[512] = "";

        /**
         * %llx-%llx : début et fin d'adresse
         * %4s : les permissions (rwxp)
         * %llx : l'offset
         * %*x:%*x : on ignore le device (le * dit de ne pas stocker)
         * %ld : le numéro d'inode
         * %s : le pathname s'il existe
         */

        int fields = sscanf(line, "%llx-%llx %4s %llx %*x:%*x %ld %s", &start,
                            &end, perms, &offset, &inode, pathname);
        if (fields < 5)
            continue;

        // Marquage [*] suspicious (si la zone est w et x)
        if (perms[1] == 'w' && perms[2] == 'x') {
            printf("[*] suspicious");
        }

        // Affichage des infos extraites
        printf(
            "%012llx-%012llx %s %08llx %ld %s\n", // %012llx donne l'hexadécimal
                                                  // sur 12 caractères avec des
                                                  // zéros au début
            start, end, perms, offset, inode, pathname);
    }

    fclose(f);
    return 0;
#endif
}