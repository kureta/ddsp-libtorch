#include <torch/script.h> // One-stop header.

#include <iostream>
#include <unistd.h>
#include <jack/jack.h>
#include <csignal>

jack_port_t *output_port1, *output_port2;
jack_client_t *client;

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

#define TABLE_SIZE   (200)
typedef struct {
    float sine[TABLE_SIZE];
    int left_phase;
    int right_phase;
} paTestData;

bool isRunning = true;

static void signal_handler(int sig) {
    jack_client_close(client);
    fprintf(stderr, "signal received, exiting ...\n");
    isRunning = false;
}

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This client follows a simple rule: when the JACK transport is
 * running, copy the input port to the output.  When it stops, exit.
 */

int process(jack_nframes_t nframes, void *arg) {
    jack_default_audio_sample_t *out1, *out2;
    auto *data = (paTestData *) arg;
    int i;

    out1 = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port1, nframes);
    out2 = (jack_default_audio_sample_t *) jack_port_get_buffer(output_port2, nframes);

    for (i = 0; i < nframes; i++) {
        out1[i] = data->sine[data->left_phase];  /* left */
        out2[i] = data->sine[data->right_phase];  /* right */
        data->left_phase += 1;
        if (data->left_phase >= TABLE_SIZE) data->left_phase -= TABLE_SIZE;
        data->right_phase += 3; /* higher pitch so we can distinguish left and right. */
        if (data->right_phase >= TABLE_SIZE) data->right_phase -= TABLE_SIZE;
    }

    return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown(void *arg) {
    exit(1);
}


int main(int argc, const char *argv[]) {
    torch::jit::script::Module net;
    try {
        // Deserialize the ScriptModule from a file using torch::jit::load().
        net = torch::jit::load("/home/kureta/Documents/repos/personal/ddsp-pytorch/zak.pt");
    }
    catch (const c10::Error &e) {
        std::cerr << "error loading the model\n";
        return -1;
    }

//    std::cout << net.dump_to_str(false, false, false) << std::endl;
    std::cout << "ok\n";

    auto tensor_options = torch::TensorOptions()
            .device(torch::kCUDA, 0);

    torch::Tensor f0s = torch::ones({1, 1, 1}, tensor_options) * 440.f;
    torch::Tensor amps = torch::ones({1, 1, 1}, tensor_options) * 66.f;
    std::vector<torch::jit::IValue> inputs = {f0s, amps};

    torch::Tensor output = net.forward(inputs).toTensor().to(torch::kCPU);
    auto buffer = static_cast<float *>(output.data_ptr());

    std::cout << buffer[0] << std::endl;

    const char **ports;
    const char *client_name;
    const char *server_name = nullptr;
    jack_options_t options = JackNullOption;
    jack_status_t status;
    paTestData data;
    int i;

    if (argc >= 2) {        /* client name specified? */
        client_name = argv[1];
        if (argc >= 3) {    /* server name specified? */
            server_name = argv[2];
            int my_option = JackNullOption | JackServerName;
            options = (jack_options_t) my_option;
        }
    } else {            /* use basename of argv[0] */
        client_name = strrchr(argv[0], '/');
        if (client_name == nullptr) {
            client_name = argv[0];
        } else {
            client_name++;
        }
    }

    for (i = 0; i < TABLE_SIZE; i++) {
        data.sine[i] = 0.2f * (float) sin(((double) i / (double) TABLE_SIZE) * M_PI * 2.);
    }
    data.left_phase = data.right_phase = 0;


    /* open a client connection to the JACK server */

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

    /* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

    jack_set_process_callback(client, process, &data);

    /* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	*/

    jack_on_shutdown(client, jack_shutdown, nullptr);

    /* create two ports */

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

    /* Tell the JACK server that we are ready to roll.  Our
	 * process() callback will start running now. */

    if (jack_activate(client)) {
        fprintf(stderr, "cannot activate client");
        exit(1);
    }

    /* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */

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
