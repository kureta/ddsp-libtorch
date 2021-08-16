#include <torch/script.h> // One-stop header.

#include <iostream>
#include <csignal>
#include "JackConnection.h"

bool isRunning = true;
bool *isNew = new bool;

torch::jit::script::Module net = torch::jit::load("/home/kureta/Documents/repos/personal/ddsp-pytorch/zak.pt");
const torch::Tensor f0s = torch::ones({1, 1, 1}) * 220.f;
const torch::Tensor amps = torch::ones({1, 1, 1}) * .95;
const std::vector<torch::jit::IValue> inputs = {f0s.to(torch::kCPU), amps.to(torch::kCPU)};
float buffer[512];

JackConnection jack;

static void signal_handler(int sig) {
    jack.close();
    fprintf(stderr, "signal received, exiting ...\n");
    isRunning = false;
}

static void inference_loop() {
    torch::NoGradGuard no_grad;
    while (isRunning) {
        while (*isNew && isRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const torch::Tensor output = net.forward(inputs).toTensor().to(torch::kCPU);
        auto result = output.contiguous().data_ptr<float>();
        std::memcpy(buffer, result, sizeof(float) * 512);

        *isNew = true;
    }
}

int main(int argc, const char *argv[]) {
    /* install a signal handler to properly quits jack client */
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);

    *isNew = false;
    std::thread inference(inference_loop);

    jack.start(isNew, &buffer[0]);

    /* keep running until the Ctrl+C */
    while (isRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    inference.join();
}
