1. Execution
    You can directly run 
        $ make
    to generate desire executables.

2. Description
    Since the messages sent between are all processes in plaintext form, it's more
    convinient to use format string to make correspond string. Thus, whenever I generate a 
    file descriptor I always wrap it as stream for calling fprintf(). However, since stream
    may contain some buffer, which store the messages we want to sent, not directly sent
    to the destination, it might cause some problem. So we need to fflush() it whenever we 
    want to send any message.
    To avoid different root_host simultaneously sent message to Host.FIFO, the root_host who
    want to write message to Host.FIFO should acquire write lock at first.