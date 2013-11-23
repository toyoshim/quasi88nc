#include <fcntl.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>

#include "SDL.h"
#include "SDL_nacl.h"

#include "naclfs.h"

class Quasi88Instance : public pp::Instance {
 private:
  pthread_t main_thread_;
  naclfs::NaClFs* naclfs_;
  bool ready_;
  bool started_;

  static void* Start(void* arg) {
    const char* argv[] = { "Quasi88", NULL };
    puts("starting quasi88 main...");
    SDL_main(1, const_cast<char**>(argv));
    return 0;
  }

 public:
  explicit Quasi88Instance(PP_Instance instance)
      : pp::Instance(instance),
	naclfs_(NULL),
	ready_(false),
	started_(false) {
  }

  virtual ~Quasi88Instance() {
    if (main_thread_) { pthread_join(main_thread_, NULL); }
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    // Initialize naclfs.
    naclfs_ = new naclfs::NaClFs(this);
    naclfs_->Log("naclfs trace on");
    naclfs_->set_trace(true);
    naclfs_->Log("initializing stdio...");
    open("/dev/stdin", O_RDONLY);
    open("/dev/stdout", O_WRONLY);
    open("/dev/stderr", O_WRONLY);
    naclfs_->Log("done\n");
    puts("stdout connect");

    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
    RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);

    SDL_NACL_SetInstance(pp_instance(), 640, 420);

    ready_ = true;
    return true;
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    if (!ready_)
      return true;

    SDL_NACL_PushEvent(event);
    return true;
  }

  virtual void HandleMessage(const pp::Var& var_message) {
    if (!ready_)
      return;

    const char start[] = "_start_";
    const char key[] = "_key_";
    const char config[] = "_config_";
    std::string message = var_message.AsString();
    int code = -1;
    if (!started_ && message.compare(start) == 0) {
      started_ = true;
      pthread_create(&main_thread_, NULL, Start, NULL);
      return;
    }
    if (message.compare(config) == 0) {
      code = 123;
    } else if (message.find(key) == 0) {
      code = atoi(&message[sizeof(key) - 1]);
    }
    if (code > 0) {
      printf("pseudo key input: %d\n", code);
      pp::KeyboardInputEvent keydown(this,
				     PP_INPUTEVENT_TYPE_KEYDOWN,
				     0,
				     0,
				     code,
				     pp::Var(""));
      SDL_NACL_PushEvent(keydown);
      pp::KeyboardInputEvent keyup(this,
				   PP_INPUTEVENT_TYPE_KEYUP,
				   0,
				   0,
				   code,
				   pp::Var(""));
      SDL_NACL_PushEvent(keyup);
    } else {
      naclfs_->HandleMessage(var_message);
    }
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
