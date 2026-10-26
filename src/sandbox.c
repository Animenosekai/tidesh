/** sandbox.c
 *
 * Master coordination module for filesystem and network sandboxing in tidesh.
 * Dispatches and coordinates filesystem isolation (Landlock, Seatbelt, chroot)
 * and network isolation (air-gapping, proxy filtering, port/domain rules).
 */

#include "sandbox.h"
#include "session.h"

void sandbox_apply_child(Session *session) {
    if (!session)
        return;

    fs_sandbox_apply_child(session);
    net_sandbox_apply_child(session);
}

bool sandbox_is_active(const Session *session) {
    if (!session)
        return false;

    if (session->fs_sandbox && session->fs_sandbox->is_active)
        return true;
    if (session->net_sandbox && session->net_sandbox->is_active)
        return true;

    return false;
}

void sandbox_free_all(Session *session) {
    if (!session)
        return;

    if (session->fs_sandbox) {
        fs_sandbox_free_stack(session->fs_sandbox);
        session->fs_sandbox = NULL;
    }
    if (session->net_sandbox) {
        net_sandbox_free_stack(session->net_sandbox);
        session->net_sandbox = NULL;
    }
}
