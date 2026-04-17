#ifndef RTM_MONITOR_ACTIVITY_H
#define RTM_MONITOR_ACTIVITY_H

namespace rtm
{
    // Thread-safe. A bare glfwPostEmptyEvent would be dropped as a spurious
    // Wayland frame-callback wake — this also bumps the activity counter.
    void request_redraw();
}

#endif
