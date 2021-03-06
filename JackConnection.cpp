//
// Created by kureta on 8/6/21.
//

#include <cstdio>
#include <iostream>
#include "JackConnection.h"

int process_callback(jack_nframes_t nframes, void *arg) {
    return static_cast<JackConnection *>(arg)->process(nframes);
}

void JackConnection::close() {
    jack_client_close(this->client);
}

int JackConnection::process(jack_nframes_t nframes) {
    if (!*this->isNew) {
        std::cout << " XRUN!!!!" << std::endl;
    }

    jack_default_audio_sample_t *out1, *out2;

    out1 = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port1, nframes);
    out2 = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port2, nframes);

    for (int i = 0; i < nframes; i++) {
        out1[i] = this->buffer[i];
        out2[i] = this->buffer[i];
    }

    *this->isNew = false;
    return 0;
}

JackConnection::JackConnection() {
    const char *server_name = nullptr;
    const char *client_name = "zak-rt";
    jack_options_t options = JackNullOption;
    jack_status_t status;

    client = jack_client_open(client_name, options, &status, server_name);
    if (client == nullptr) {
        fprintf(stderr, "jack_client_open() failed, "
                        "status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            fprintf(stderr, "Unable to connect to JACK server\n");
        }
        exit(1);
    }
    if (status & JackServerStarted) {
        fprintf(stderr, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
        client_name = jack_get_client_name(client);
        fprintf(stderr, "unique name `%s' assigned\n", client_name);
    }

    jack_set_process_callback(client, process_callback, this);
    jack_on_shutdown(client, jack_shutdown, nullptr);

    output_port1 = jack_port_register(client, "output1",
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput, 0);

    output_port2 = jack_port_register(client, "output2",
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput, 0);

    if ((output_port1 == nullptr) || (output_port2 == nullptr)) {
        fprintf(stderr, "no more JACK ports available\n");
        exit(1);
    }
}

void JackConnection::start(bool *_is_new, float *_buffer) {
    this->isNew = _is_new;
    this->buffer = _buffer;

    if (jack_activate(client)) {
        fprintf(stderr, "cannot activate client");
        exit(1);
    }

    ports = jack_get_ports(client, nullptr, nullptr,
                           JackPortIsPhysical | JackPortIsInput);
    if (ports == nullptr) {
        fprintf(stderr, "no physical playback ports\n");
        exit(1);
    }

    if (jack_connect(client, jack_port_name(output_port1), ports[0])) {
        fprintf(stderr, "cannot connect output ports\n");
    }

    if (jack_connect(client, jack_port_name(output_port2), ports[1])) {
        fprintf(stderr, "cannot connect output ports\n");
    }

    jack_free(ports);
}
