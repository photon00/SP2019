#Note: In my code I use macro define DATABASE "./account_list", thus the account file should be placed at the same folder of the executables.

1. Implement IO multiplexing
    Calling select() to detect available read for listening socket and others connected socket. If listening socket is triggered it would call accept()
    and return a new file descriptor for messages exchange, after that we need to increase the fd that select() detected.
    Once select() return we check the correspond file descriptor and perform corresponding tasks.

2. file lock
    When access the file always acquire lock first, read lock for read server; write lock for write server. However, since multiple write request should be
    handled in the same process, we need other mechanism dealing with write lock, so I use a simple array keep track on the id, whose content is modifying.