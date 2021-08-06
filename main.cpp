#include <torch/script.h> // One-stop header.

#include <iostream>
#include <unistd.h>
#include <csignal>
#include "JackConnection.h"

bool isRunning = true;
// TODO: Getrid of this global variable.
JackConnection jack;

static void signal_handler(int sig) {
    jack.close();
    fprintf(stderr, "signal received, exiting ...\n");
    isRunning = false;
}

int main(int argc, const char *argv[]) {
    jack.start();

    /* install a signal handler to properly quits jack client */
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);

    /* keep running until the Ctrl+C */

    while (isRunning) {
        sleep(1);
    }
}
