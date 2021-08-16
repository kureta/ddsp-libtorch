//
// Created by kureta on 8/6/21.
//

#ifndef PYTROCH_DDSP_RT_JACKCONNECTION_H
#define PYTROCH_DDSP_RT_JACKCONNECTION_H

#include <jack/jack.h>
#include <cstdlib>
#include <valarray>
#include <vector>


#ifndef M_PI
#define M_PI  (3.14159265)
#endif

#define TABLE_SIZE   (200)
typedef struct {
    float sine[TABLE_SIZE];
    int left_phase;
    int right_phase;
} paTestData;


class JackConnection {
private:
    jack_port_t *output_port1, *output_port2;
    jack_client_t *client;
    const char **ports;
    paTestData data;
    bool* isNew;
    float* buffer;

    static void jack_shutdown(void *arg) {
        exit(1);
    }

public:
    JackConnection();

    void close();

    void start();

    int process(jack_nframes_t nframes);
};


#endif //PYTROCH_DDSP_RT_JACKCONNECTION_H
