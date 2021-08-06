//
// Created by kureta on 8/6/21.
//

#include <cstdio>
#include "JackConnection.h"

int process_callback(jack_nframes_t nframes, void *arg) {
    return static_cast<JackConnection*>(arg)->process(nframes);
}

void JackConnection::close() {
    jack_client_close(this->client);
}

int JackConnection::process(jack_nframes_t nframes) {
    jack_default_audio_sample_t *out1, *out2;
    int i;

    // TODO: There is a CUDA memory leak. I wonder where?
    std::vector<torch::jit::IValue> inputs = {f0s.to(torch::kCUDA), amps.to(torch::kCUDA)};
    torch::Tensor output = net.forward(inputs).toTensor().to(torch::kCPU);
    auto buffer = static_cast<float *>(output.data_ptr());

    out1 = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port1, nframes);
    out2 = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port2, nframes);

    // TODO: uses sine waves to control the synth with exact same parameters as python
    for (i = 0; i < nframes; i++) {
        out1[i] = buffer[i];
        out2[i] = buffer[i];
//        out1[i] = data.sine[data.left_phase];  /* left */
//        out2[i] = data.sine[data.right_phase];  /* right */
//        data.left_phase += 1;
//        if (data.left_phase >= TABLE_SIZE) data.left_phase -= TABLE_SIZE;
//        data.right_phase += 3; /* higher pitch so we can distinguish left and right. */
//        if (data.right_phase >= TABLE_SIZE) data.right_phase -= TABLE_SIZE;
    }

    return 0;
}

JackConnection::JackConnection() {
    const char *server_name = nullptr;
    const char *client_name = "zak-rt";
    jack_options_t options = JackNullOption;
    jack_status_t status;

    for (int i = 0; i < TABLE_SIZE; i++) {
        data.sine[i] = 0.2f * (float) sin(((double) i / (double) TABLE_SIZE) * M_PI * 2.);
    }
    data.left_phase = data.right_phase = 0;

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

    try {
        // Deserialize the ScriptModule from a file using torch::jit::load().
        net = torch::jit::load("/home/kureta/Documents/repos/personal/ddsp-pytorch/zak.pt");
    }
    catch (const c10::Error &e) {
        std::cerr << "error loading the model\n";
        exit(-1);
    }
}

void JackConnection::start() {
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
