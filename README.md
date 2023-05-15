#Assignment 4 directory

This directory contains source code and other files for Assignment 4.

Use this README document to store notes about design, testing, and
questions you have while developing your assignment.

# Starting Brainstorm
I will start by implementing the GET function as directed.
This will be accomplished by looking at my own asgn2, how PUT is implemented in the given httpserver, looking through the given header files for helper functions.

# Design Notes 
For this assignment, we had to implement the following:

### getopt()
In order to take in the number of threads available, we used a switch statement with getopt() to acquire optional arguments. leftover arguments were iterated through (though we did not need to as there would be only 1 argument leftover) in order to extract the port number.

### GET 
Imitating the implementation of my asgn2's GET and adapting to the given starting code, I implemented GET according to the steps directed.

### Threads
Created a worker thread with a while(true) loop, where it pops from a global queue to obtain a socket file descriptor. If the queue is empty, aka when there are no requests to work on, the queue will inherently cause the thread to sleep because of the synchronization primitives built in. It then calls handle_connection, where all synchronization will be taken care of in the subsequent functions. The worker then closes the thread, then loops back around to take the next job or sleep.

### flock()
I use this after opening file threads to establish reader/writer locks; this is because concurrent GETs should be allowed to read, while PUTs should be stricter with the access to prevent race conditions.

### Global Lock
Used to protect file creation in handle_put. This way, prevents any issues from arising when it comes to concurrent PUTS creating and overwriting files. the file is locked with flock() before the mutex thread is unlocked to help prevent more potential conflicts.
