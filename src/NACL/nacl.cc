#include <fcntl.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>

#include "SDL.h"
#include "SDL_nacl.h"

#include "naclfs.h"

extern "C" int SDL_main(int argc, char** argv);

class Quasi88Instance : public pp::Instance {
 private:
  pthread_t main_thread_;
  naclfs::NaClFs* naclfs_;

  static void* Start(void* arg) {
    const char* argv[] = { "Quasi88", NULL };
    puts("starting quasi88 main...");
    SDL_main(1, const_cast<char**>(argv));
    return 0;
  }

 public:
  explicit Quasi88Instance(PP_Instance instance)
      : pp::Instance(instance),
        naclfs_(new naclfs::NaClFs(this)) {
    naclfs_->set_trace(true);
    open("/dev/stdin", O_RDONLY);
    open("/dev/stdout", O_WRONLY);
    open("/dev/stderr", O_WRONLY);
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE |
                       PP_INPUTEVENT_CLASS_KEYBOARD);
  }

  virtual ~Quasi88Instance() {
    if (main_thread_) { pthread_join(main_thread_, NULL); }
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    SDL_NACL_SetInstance(pp_instance(), 640, 420);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    pthread_create(&main_thread_, NULL, Start, NULL);
    return true;
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    SDL_NACL_PushEvent(event);
    return true;
  }

  virtual void HandleMessage(const pp::Var& var_message) {
    naclfs_->HandleMessage(var_message);
  }
};

class Quasi88Module : public pp::Module {
 public:
  Quasi88Module() : pp::Module() {}
  virtual ~Quasi88Module() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Quasi88Instance(instance);
  }
};

namespace pp {
Module* CreateModule() {
  return new Quasi88Module();
}
}  // namespace pp
