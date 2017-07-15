# shm_transport
An attempt to communicate with shared memory for ROS 1.

    ROS 1 use socket as a communication method. If the publisher and subscriber belong to different processes of a same machine, the socket will go through loopback with AF_INET family (either UDP or TCP protocol).
    The main purpose of this project is to use shared memory IPC instead of loopback socket to establish publisher/subscriber communication. And we employ boost::interprocess::managed_shared_memory to accomplish it.
    But shared memory is lack of synchronization method like poll/epoll for socket. Even inotify does not support generating a notification when the shared memory region is written or updated. Therefore, we use the origin transport (udp or tcp) to send the handle (essentially, an offset pointer) from publishers to subscribers.
    The hardest part is life-time management. We need to deallocate a message when no subscribers use it (as soon as possible). We are still improving this issue.
