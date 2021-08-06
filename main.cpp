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
    torch::jit::script::Module net;
    try {
        // Deserialize the ScriptModule from a file using torch::jit::load().
        net = torch::jit::load("/home/kureta/Documents/repos/personal/ddsp-pytorch/zak.pt");
    }
    catch (const c10::Error &e) {
        std::cerr << "error loading the model\n";
        return -1;
    }

    std::cout << "ok\n";

    auto tensor_options = torch::TensorOptions()
            .device(torch::kCUDA, 0);

    torch::Tensor f0s = torch::ones({1, 1, 1}, tensor_options) * 440.f;
    torch::Tensor amps = torch::ones({1, 1, 1}, tensor_options) * 66.f;
    std::vector<torch::jit::IValue> inputs = {f0s, amps};

    torch::Tensor output = net.forward(inputs).toTensor().to(torch::kCPU);
    auto buffer = static_cast<float *>(output.data_ptr());

    std::cout << buffer[0] << std::endl;

    jack = JackConnection();
    jack.init(&inputs, &net);
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
