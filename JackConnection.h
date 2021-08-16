//
// Created by kureta on 8/6/21.
//

#ifndef PYTROCH_DDSP_RT_JACKCONNECTION_H
#define PYTROCH_DDSP_RT_JACKCONNECTION_H

#include <jack/jack.h>
#include <cstdlib>
#include <valarray>
#include <vector>


class JackConnection {
private:
    jack_port_t *output_port1, *output_port2;
    jack_client_t *client;
    const char **ports;
    bool *isNew;
    float *buffer;

    static void jack_shutdown(void *arg) {
        exit(1);
    }

public:
    JackConnection();

    void close();

    void start(bool *_is_new, float *_buffer);

    int process(jack_nframes_t nframes);
};


#endif //PYTROCH_DDSP_RT_JACKCONNECTION_H
