#include <chrono>
#include <algorithm>
#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
 
#include "core/rpicam_encoder.hpp"
#include "output/output.hpp"


#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"
 
 using namespace std::placeholders;
 
 // Some keypress/signal handling.
 
 static int signal_received;
 static void default_signal_handler(int signal_number)
 {
     signal_received = signal_number;
     LOG(1, "Received signal " << signal_number);
 }
 
 static int get_key_or_signal(VideoOptions const *options, pollfd p[1])
 {
     int key = 0;
     if (signal_received == SIGINT)
         return 'x';
     if (options->keypress)
     {
         poll(p, 1, 0);
         if (p[0].revents & POLLIN)
         {
             char *user_string = nullptr;
             size_t len;
             [[maybe_unused]] size_t r = getline(&user_string, &len, stdin);
             key = user_string[0];
         }
     }
     if (options->signal)
     {
         signal_received = 0;
     }
     return key;
 }
 
 
class MyOutput : public Output {
public:
   MyOutput(VideoOptions const *options): Output(options) {
     std::string codec(options->codec);
     std::transform(codec.begin(), codec.end(), codec.begin(), ::toupper);
     V4L2DeviceParameters outparam(options->output.c_str(), V4l2Device::fourcc(codec.c_str()), options->width, options->height, 0, IOTYPE_MMAP, 255);
     m_videoOutput = V4l2Output::create(outparam);
   }

   void outputBuffer(void *mem, size_t size, int64_t timestamp_us, uint32_t flags) override {
         LOG(2, "outputBuffer: " << size);
         if (m_videoOutput) {
		m_videoOutput->write((char*)mem, size);
         }
   }
private:
  V4l2Output* m_videoOutput;
};

 // The main even loop for the application.
 
 static void event_loop(RPiCamEncoder &app)
 {
     VideoOptions const *options = app.GetOptions();
     std::unique_ptr<Output> output = std::unique_ptr<Output>(new MyOutput(options));
     app.SetEncodeOutputReadyCallback(std::bind(&Output::OutputReady, output.get(), _1, _2, _3, _4));
     app.SetMetadataReadyCallback(std::bind(&Output::MetadataReady, output.get(), _1));
 
     app.OpenCamera();
     app.ConfigureVideo(RPiCamEncoder::FLAG_VIDEO_NONE);
     app.StartEncoder();
     app.StartCamera();
 
     // Monitoring for keypresses and signals.
     signal(SIGINT, default_signal_handler);
     pollfd p[1] = { { STDIN_FILENO, POLLIN, 0 } };

 
     for (unsigned int count = 0; ; count++)
     {
         RPiCamEncoder::Msg msg = app.Wait();
         if (msg.type == RPiCamApp::MsgType::Timeout)
         {
             LOG_ERROR("ERROR: Device timeout detected, attempting a restart!!!");
             app.StopCamera();
             app.StartCamera();
             continue;
         }
         if (msg.type == RPiCamEncoder::MsgType::Quit)
             return;
         else if (msg.type != RPiCamEncoder::MsgType::RequestComplete)
             throw std::runtime_error("unrecognised message!");
         int key = get_key_or_signal(options, p);
         if (key == '\n')
             output->Signal();
 
         LOG(2, "Viewfinder frame " << count);
         auto now = std::chrono::high_resolution_clock::now();
         if (key == 'x' || key == 'X')
         {
             app.StopCamera(); // stop complains if encoder very slow to close
             app.StopEncoder();
             return;
         }
         CompletedRequestPtr &completed_request = std::get<CompletedRequestPtr>(msg.payload);
         app.EncodeBuffer(completed_request, app.VideoStream());
     }
 }
 
 int main(int argc, char *argv[])
 {
     try
     {
         RPiCamEncoder app;
         VideoOptions *options = app.GetOptions();
         if (options->Parse(argc, argv))
         {
             if (options->verbose >= 2)
                 options->Print();
 
             event_loop(app);
         }
     }
     catch (std::exception const &e)
     {
         LOG_ERROR("ERROR: *** " << e.what() << " ***");
         return -1;
     }
     return 0;
 }
