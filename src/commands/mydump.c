#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#include "commands/mydump.h"

int builtin_mydump(int argc, char **argv, Session *session) {
    (void)session;
    // Vérification des arguments : on attend "-p <PID> --start 0x... --end
    // 0x... -o <file>"
    if (argc < 9) {
        fprintf(stderr,
                "Usage: mydump -p <PID> --start 0x... --end 0x... -o <file>\n");
        return 1;
    }

    char *pid_str = argv[2];
    // strtoull convertit l'hexadécimal (base 16) en nombre 64 bits
    unsigned long long start       = strtoull(argv[4], NULL, 16);
    unsigned long long end         = strtoull(argv[6], NULL, 16);
    char              *output_path = argv[8];

    if (end <= start) {
        fprintf(stderr, "Erreur: Adresse de fin invalide.\n");
        return 1;
    }

#if defined(__APPLE__)
    // Create output file first
    int fd_out = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror("mydump (open output)");
        return 1;
    }

    int           pid = atoi(pid_str);
    task_t        task;
    kern_return_t kr = task_for_pid(mach_task_self(), pid, &task);
    if (kr != KERN_SUCCESS) {
        fprintf(
            stderr,
            "mydump: failed to get task for pid %d (error %d). need sudo?\n",
            pid, kr);
        close(fd_out);
        return 1;
    }

    mach_vm_address_t address         = (mach_vm_address_t)start;
    mach_vm_size_t    size            = (mach_vm_size_t)(end - start);
    mach_vm_size_t    bytes_read_mach = 0;

    char   buffer[4096];
    size_t total_copied = 0;

    while (total_copied < size) {
        size_t chunk =
            (size - total_copied > 4096) ? 4096 : (size - total_copied);
        // mach_vm_read_overwrite is generally better than mach_vm_read as we
        // provide the buffer
        kr =
            mach_vm_read_overwrite(task, address + total_copied, chunk,
                                   (mach_vm_address_t)buffer, &bytes_read_mach);

        if (kr != KERN_SUCCESS) {
            // If we fail specifically on a page, we break
            break;
        }

        write(fd_out, buffer, bytes_read_mach);
        total_copied += bytes_read_mach;
    }

    printf("Acquisition terminée : %zu octets copiés dans %s\n", total_copied,
           output_path);
    close(fd_out);
    return 0;

#else
    char mem_path[256];
    snprintf(mem_path, sizeof(mem_path), "/proc/%s/mem", pid_str);

    /**
     * On ne peut pas lire le fichier du début à la fin car il est
     * trop gros et contient des zones non allouées donc on doit se
     * déplacer avec lseek à l'adresse de début demandée avant de lire
     */

    // Ouvrir la mémoire du processus
    int fd_mem = open(mem_path, O_RDONLY);
    if (fd_mem == -1) {
        perror("mydump (open mem)");
        return 1;
    }

    // Se placer à l'adresse de début
    if (lseek(fd_mem, (off_t)start, SEEK_SET) == -1) {
        perror("mydump (lseek)");
        close(fd_mem);
        return 1;
    }

    // Créer le fichier de destination
    int fd_out = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror("mydump (open output)");
        close(fd_mem);
        return 1;
    }

    // Copier les données par blocs de 4Ko (taille standard d'une page sous
    // Linux)
    char   buffer[4096];
    size_t to_copy      = end - start;
    size_t total_copied = 0;

    while (total_copied < to_copy) {
        size_t chunk =
            (to_copy - total_copied > 4096) ? 4096 : (to_copy - total_copied);
        ssize_t n_read = read(fd_mem, buffer, chunk);

        if (n_read <= 0)
            break; // Erreur ou zone non lisible

        write(fd_out, buffer, n_read);
        total_copied += n_read;
    }

    printf("Acquisition terminée : %zu octets copiés dans %s\n", total_copied,
           output_path);

    close(fd_mem);
    close(fd_out);
    return 0;
#endif
}