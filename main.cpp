#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <errno.h>
#include <sstream>
#include <string>
#include <map>
#include <thread>
#include <v8.h>
#include <ml_logging.h>
#include <ml_lifecycle.h>
#include <ml_privileges.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <jni.h>

#include <iostream>

#define LOG_TAG "exokit"
#define application_name LOG_TAG

using namespace v8;

jint JNI_OnLoad(JavaVM* aVm, void* aReserved) {
  std::cout << "main JNI_OnLoad " << (void *)aVm << " " << aReserved << std::endl;
  return JNI_VERSION_1_6;
}

namespace node {
  extern std::map<std::string, void *> dlibs;
  int Start(int argc, char* argv[]);
}
int stdoutfds[2];
int stderrfds[2];

#include "build/libexokit/dlibs.h"

/* struct application_context_t {
  int dummy_value;
};
enum DummyValue {
  STOPPED = 0,
  RUNNING,
  PAUSED,
};

static void onNewInitArg(void* application_context) {
  MLLifecycleInitArgList *args;
  MLLifecycleGetInitArgList(&args);

  ((struct application_context_t*)application_context)->dummy_value = DummyValue::RUNNING;
  ML_LOG_TAG(Info, LOG_TAG, "%s: On new init arg called %x.", application_name, args);
}

static void onStop(void* application_context) {
  ((struct application_context_t*)application_context)->dummy_value = DummyValue::STOPPED;
  ML_LOG_TAG(Info, LOG_TAG, "%s: On stop called.", application_name);
}

static void onPause(void* application_context) {
  ((struct application_context_t*)application_context)->dummy_value = DummyValue::PAUSED;
  ML_LOG_TAG(Info, LOG_TAG, "%s: On pause called.", application_name);
}

static void onResume(void* application_context) {
  ((struct application_context_t*)application_context)->dummy_value = DummyValue::RUNNING;
  ML_LOG_TAG(Info, LOG_TAG, "%s: On resume called.", application_name);
}

static void onUnloadResources(void* application_context) {
  ((struct application_context_t*)application_context)->dummy_value = DummyValue::STOPPED;
  ML_LOG_TAG(Info, LOG_TAG, "%s: On unload resources called.", application_name);
} */

/* extern "C" {
  void node_register_module_exokit(Local<Object> exports, Local<Value> module, Local<Context> context);
  void node_register_module_vm_one(Local<Object> exports, Local<Value> module, Local<Context> context);
  void node_register_module_raw_buffer(Local<Object> exports, Local<Value> module, Local<Context> context);
  void node_register_module_child_process_thread(Local<Object> exports, Local<Value> module, Local<Context> context);
}

inline void registerDlibs(std::map<std::string, void *> &dlibs) {
  dlibs["/package/build/Release/exokit.node"] = (void *)&node_register_module_exokit;
  dlibs["/package/node_modules/vm-one/build/Release/vmOne.node"] = (void *)&node_register_module_vm_one;
  dlibs["/package/node_modules/raw-buffer/build/Release/raw_buffer.node"] = (void *)&node_register_module_raw_buffer;
  dlibs["/package/node_modules/child-process-thread/build/Release/child_process_thread.node"] = (void *)&node_register_module_child_process_thread;
} */

__attribute__ ((visibility ("default"))) void checkBoot() {
  std::cout << "check boot" << std::endl;
}

int mkdirp(const char *path)
{
  /* Adapted from http://stackoverflow.com/a/2336245/119527 */
  const size_t len = strlen(path);
  char _path[PATH_MAX];
  char *p; 

  errno = 0;

  /* Copy string so its mutable */
  if (len > sizeof(_path)-1) {
      errno = ENAMETOOLONG;
      return -1; 
  }   
  strcpy(_path, path);

  /* Iterate the string */
  for (p = _path + 1; *p; p++) {
    if (*p == '/') {
      /* Temporarily truncate */
      *p = '\0';

      if (mkdir(_path, S_IRWXU) != 0) {
        if (errno != EEXIST)
          return -1; 
      }

      *p = '/';
    }
  }   

  if (mkdir(_path, S_IRWXU) != 0) {
    if (errno != EEXIST)
      return -1; 
  }   

  return 0;
}

int fscopy(const char *src, const char *dst) {
  /* SOURCE */
  int sfd = open(src, O_RDONLY);
  std::cout << "fscopy inner 0 " << src << " " << sfd << " " << errno << std::endl;
  size_t filesize = lseek(sfd, 0, SEEK_END);

  std::cout << "fscopy inner 1 " << src << " " << sfd << " " << filesize << " " << errno << std::endl;
  
  void *srcMem = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, sfd, 0);
  
  std::cout << "fscopy inner 2 " << srcMem << " " << errno << std::endl;

  /* DESTINATION */
  int dfd = open(dst, O_RDWR | O_CREAT, 0666);
  
  std::cout << "fscopy inner 3 " << dst << " " << dfd << " " << errno << std::endl;

  ftruncate(dfd, filesize);

  void *dstMem = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, dfd, 0);
  
  std::cout << "fscopy inner 4 " << dstMem << " " << errno << std::endl;

  /* COPY */

  memcpy(dstMem, srcMem, filesize);

  munmap(srcMem, filesize);
  munmap(dstMem, filesize);

  close(sfd);
  close(dfd);

  return 0;
}

__attribute__ ((visibility ("default"))) void doBoot() {
  {
    setenv("ANDROID_ROOT", "/package/system", 1);
    setenv("ANDROID_DATA", "/tmp", 1);
    setenv("BOOTCLASSPATH", "/package:.", 1);
    setenv("CLASSPATH", "/package:.", 1);
  }
  {
    int result = mkdirp("/tmp/dalvik-cache/arm64");
    std::cout << "mkdirp result " << result << std::endl;
  }
  {
    int result = fscopy("/package/system/framework/arm64/boot.art", "/tmp/dalvik-cache/arm64/package@system@framework@boot.art");
    std::cout << "fscopy 1 result " << result << std::endl;
  }
  {
    int result = fscopy("/package/system/framework/arm64/boot.oat", "/tmp/dalvik-cache/arm64/package@system@framework@boot.oat");
    std::cout << "fscopy 2 result " << result << std::endl;
  }
  
  std::cout << "init lib 0" << std::endl;
  void *libandroid_runtime_dso = dlopen("libart.so", RTLD_NOW);
  std::cout << "init lib 1 " << libandroid_runtime_dso << std::endl;
  // jint (*JNI_GetDefaultJavaVMInitArgs)(void*) = (jint (*)(void*))dlsym(libandroid_runtime_dso, "JNI_GetDefaultJavaVMInitArgs");
  jint (*JNI_CreateJavaVM)(JavaVM**, JNIEnv**, void*) = (jint (*)(JavaVM**, JNIEnv**, void*))dlsym(libandroid_runtime_dso, "JNI_CreateJavaVM");
  jint (*JNI_GetCreatedJavaVMs)(JavaVM**, jsize, jsize*) = (jint (*)(JavaVM**, jsize, jsize*))dlsym(libandroid_runtime_dso, "JNI_GetCreatedJavaVMs");
  std::cout << "init lib 2 " << (void *)JNI_CreateJavaVM << " " << JNI_GetCreatedJavaVMs << std::endl;
  std::cout << "init lib X " << "\n" <<
    "JNI_CreateJavaVM b *" << dlsym(libandroid_runtime_dso, "JNI_CreateJavaVM") << "\n" <<
    
    "_ZN3art2OS10FileExistsEPKc b *" << dlsym(libandroid_runtime_dso, "_ZN3art2OS10FileExistsEPKc") << "\n" <<
    
    "_ZN3art14GetAndroidRootEv b *" << dlsym(libandroid_runtime_dso, "_ZN3art14GetAndroidRootEv") << "\n" <<
    
    "_ZN7android22InitializeNativeLoaderEv b *" << dlsym(libandroid_runtime_dso, "_ZN7android22InitializeNativeLoaderEv") << "\n" <<
    "_ZN3art7Runtime6CreateEONS_18RuntimeArgumentMapE b *" << dlsym(libandroid_runtime_dso, "_ZN3art7Runtime6CreateEONS_18RuntimeArgumentMapE") << "\n" <<
    "_ZN3art7Runtime6CreateERKNSt3__16vectorINS1_4pairINS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEPKvEENS7_ISC_EEEEb b *" << dlsym(libandroid_runtime_dso, "_ZN3art7Runtime6CreateERKNSt3__16vectorINS1_4pairINS1_12basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEPKvEENS7_ISC_EEEEb") << "\n" <<
    "_ZN3art7Runtime5StartEv b *" << dlsym(libandroid_runtime_dso, "_ZN3art7Runtime5StartEv") << "\n" <<
    "_ZN3art7Runtime5AbortEv b *" << dlsym(libandroid_runtime_dso, "_ZN3art7Runtime5AbortEv") << "\n" <<

    "_ZN3art2gc5space10ImageSpace4InitEPKcS4_bPKNS_7OatFileEPNSt3__112basic_stringIcNS8_11char_traitsIcEENS8_9allocatorIcEEEE b *" << dlsym(libandroid_runtime_dso, "_ZN3art2gc5space10ImageSpace4InitEPKcS4_bPKNS_7OatFileEPNSt3__112basic_stringIcNS8_11char_traitsIcEENS8_9allocatorIcEEEE") << "\n" <<
    "_ZN3art2gc5space10ImageSpace15CreateBootImageEPKcNS_14InstructionSetEbPNSt3__112basic_stringIcNS6_11char_traitsIcEENS6_9allocatorIcEEEE b *" << dlsym(libandroid_runtime_dso, "_ZN3art2gc5space10ImageSpace15CreateBootImageEPKcNS_14InstructionSetEbPNSt3__112basic_stringIcNS6_11char_traitsIcEENS6_9allocatorIcEEEE") << "\n" <<
    "_ZN3art2gc5space10ImageSpace17FindImageFilenameEPKcNS_14InstructionSetEPNSt3__112basic_stringIcNS6_11char_traitsIcEENS6_9allocatorIcEEEEPbSD_SE_SE_SE_ b *" << dlsym(libandroid_runtime_dso, "_ZN3art2gc5space10ImageSpace17FindImageFilenameEPKcNS_14InstructionSetEPNSt3__112basic_stringIcNS6_11char_traitsIcEENS6_9allocatorIcEEEEPbSD_SE_SE_SE_") << "\n" <<
    
    "_ZN3art12StringPrintfEPKcz b *" << dlsym(libandroid_runtime_dso, "_ZN3art12StringPrintfEPKcz") << "\n" <<
    
    "_ZN3art14GetDalvikCacheEPKcb b *" << dlsym(libandroid_runtime_dso, "_ZN3art14GetDalvikCacheEPKcb") << "\n" <<
    "_ZN3art14GetDalvikCacheEPKcbPNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEPbSA_SA_ b *" << dlsym(libandroid_runtime_dso, "_ZN3art14GetDalvikCacheEPKcbPNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEPbSA_SA_") << "\n" <<
    "_ZN3art19GetDalvikCacheOrDieEPKcb b *" << dlsym(libandroid_runtime_dso, "_ZN3art19GetDalvikCacheOrDieEPKcb") << "\n" <<
    "_ZN3art22GetDalvikCacheFilenameEPKcS1_PNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEES9_ b *" << dlsym(libandroid_runtime_dso, "_ZN3art22GetDalvikCacheFilenameEPKcS1_PNSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEES9_") << "\n" <<
    
    "_ZN3art10LogMessage15LogLineLowStackEPKcjNS_11LogSeverityES2_ b *" << dlsym(libandroid_runtime_dso, "_ZN3art10LogMessage15LogLineLowStackEPKcjNS_11LogSeverityES2_") << "\n" <<
    "_ZN3art10LogMessage6streamEv b *" << " " << dlsym(libandroid_runtime_dso, "_ZN3art10LogMessage6streamEv") << "\n" <<
    "_ZN3art10LogMessage7LogLineEPKcjNS_11LogSeverityES2_ b *" << dlsym(libandroid_runtime_dso, "_ZN3art10LogMessage7LogLineEPKcjNS_11LogSeverityES2_") << "\n" <<
    "_ZN3art10LogMessageC1EPKcjNS_11LogSeverityEi b *" << dlsym(libandroid_runtime_dso, "_ZN3art10LogMessageC1EPKcjNS_11LogSeverityEi") << "\n" <<
    "_ZN3art10LogMessageC2EPKcjNS_11LogSeverityEi b *" << dlsym(libandroid_runtime_dso, "_ZN3art10LogMessageC2EPKcjNS_11LogSeverityEi") << "\n" <<
    "_ZN3art10LogMessageD1Ev b *" << dlsym(libandroid_runtime_dso, "_ZN3art10LogMessageD1Ev") << "\n" <<
    "_ZN3art10LogMessageD2Ev b *" << dlsym(libandroid_runtime_dso, "_ZN3art10LogMessageD2Ev") << "\n" <<
    
  std::endl;

  checkBoot();
  
  /* JavaVM *jvms[2];
  jsize inSize = 2;
  jsize outSize;
  std::cout << "init lib 3" << std::endl;
  jint result = JNI_GetCreatedJavaVMs(jvms, inSize, &outSize);
  std::cout << "init lib 4 " << result << " " << outSize << std::endl; */
  
  /* JavaVMInitArgs args;
  args.version = JNI_VERSION_1_6;
  args.options = nullptr;
  args.nOptions = 0;
  args.ignoreUnrecognized = JNI_TRUE; */
  JavaVMInitArgs args;                        // Initialization arguments
  JavaVMOption *options = new JavaVMOption[1];   // JVM invocation options
  options[0].optionString = "-Djava.class.path=/package:.";   // where to find java .class
  args.version = JNI_VERSION_1_6;             // minimum Java version
  args.nOptions = 1;                          // number of options
  args.options = options;
  args.ignoreUnrecognized = false;     // invalid options make the JVM init fail
  JavaVM *jvm;
  JNIEnv *env;
  std::cout << "init lib 5 " << (void *)JNI_CreateJavaVM << std::endl;
  jint result = JNI_CreateJavaVM(&jvm, &env, &args);
  std::cout << "init lib 6 " << result << " " << (void *)jvm << " " << (void *)env << " " << args.nOptions << std::endl;
}

constexpr size_t STDIO_BUF_SIZE = 64 * 1024;
const MLPrivilegeID privileges[] = {
  MLPrivilegeID_LowLatencyLightwear,
  MLPrivilegeID_WorldReconstruction,
  MLPrivilegeID_Occlusion,
  MLPrivilegeID_ControllerPose,
  MLPrivilegeID_CameraCapture,
  MLPrivilegeID_AudioCaptureMic,
  MLPrivilegeID_VoiceInput,
  MLPrivilegeID_AudioRecognizer,
  MLPrivilegeID_Internet,
  MLPrivilegeID_LocalAreaNetwork,
  MLPrivilegeID_BackgroundDownload,
  MLPrivilegeID_BackgroundUpload,
  MLPrivilegeID_PwFoundObjRead,
  MLPrivilegeID_NormalNotificationsUsage,
};

int main(int argc, char **argv) {
  ML_LOG_TAG(Info, LOG_TAG, "main 1");
  if (argc > 1) {
    return node::Start(argc, argv);
  }
  
  ML_LOG_TAG(Info, LOG_TAG, "main 2");

  registerDlibs(node::dlibs);

  ML_LOG_TAG(Info, LOG_TAG, "main 3");
  
  MLResult result = MLPrivilegesStartup();
  if (result != MLResult_Ok) {
    ML_LOG(Info, "failed to start privilege system %x", result);
  }
  
  ML_LOG_TAG(Info, LOG_TAG, "main 4");
  
  const size_t numPrivileges = sizeof(privileges) / sizeof(privileges[0]);
  for (size_t i = 0; i < numPrivileges; i++) {
    const MLPrivilegeID &privilege = privileges[i];
    MLResult result = MLPrivilegesCheckPrivilege(privilege);
    if (result != MLPrivilegesResult_Granted) {
      const char *s = MLPrivilegesGetResultString(result);
      ML_LOG(Info, "did not have privilege %u: %u %s", privilege, result, s);

      MLResult result = MLPrivilegesRequestPrivilege(privilege);
      if (result != MLPrivilegesResult_Granted) {
        const char *s = MLPrivilegesGetResultString(result);
        ML_LOG(Info, "failed to get privilege %u: %u %s", privilege, result, s);
      }
    }
  }
  
  ML_LOG_TAG(Info, LOG_TAG, "main 5");

  /* {
    MLLifecycleCallbacks lifecycle_callbacks = {};
    lifecycle_callbacks.on_new_initarg = onNewInitArg;
    lifecycle_callbacks.on_stop = onStop;
    lifecycle_callbacks.on_pause = onPause;
    lifecycle_callbacks.on_resume = onResume;
    lifecycle_callbacks.on_unload_resources = onUnloadResources;
    application_context_t application_context;
    MLResult lifecycle_status = MLLifecycleInit(&lifecycle_callbacks, (void*)&application_context);

    MLResult result = MLLifecycleSetReadyIndication();
    if (result == MLResult_Ok) {
      ML_LOG(Info, "lifecycle ready!");
    } else {
      ML_LOG(Error, "failed to indicate lifecycle ready: %u", result);
    }
  }

  {
    ML_LOG(Info, "------------------------------test query");

    const char *host = "google.com";
    struct addrinfo hints, *res;
    int errcode;
    char addrstr[100];
    void *ptr;

    memset (&hints, 0, sizeof (hints));
    // hints.ai_flags = AI_DEFAULT;
    // hints.ai_family = PF_UNSPEC;
    // hints.ai_socktype = SOCK_STREAM;
    // hints.ai_flags |= AI_CANONNAME;

    errcode = getaddrinfo(host, NULL, &hints, &res);
    ML_LOG (Info, "Host: %s %x %s\n", host, errcode, gai_strerror(errcode));
    if (errcode == 0) {
      while (res)
        {
          inet_ntop (res->ai_family, res->ai_addr->sa_data, addrstr, 100);

          switch (res->ai_family)
            {
            case AF_INET:
              ptr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
              break;
            case AF_INET6:
              ptr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
              break;
            }
          inet_ntop (res->ai_family, ptr, addrstr, 100);
          ML_LOG(Info, "IPv%d address: %s (%s)", res->ai_family == PF_INET6 ? 6 : 4, addrstr, res->ai_canonname);
          res = res->ai_next;
        }
    } else {
      ML_LOG(Info, "failed to getaddrinfo %x", errcode);
    }
  }

  ML_LOG(Info, "sleeping 1");

  sleep(1000);

  ML_LOG(Info, "sleeping 2"); */

  pipe(stdoutfds);
  pipe(stderrfds);

  int pid = fork();
  if (pid != 0) { // parent
    dup2(stdoutfds[1], 1);
    close(stdoutfds[0]);
    dup2(stderrfds[1], 2);
    close(stderrfds[0]);

    std::atexit([]() -> void {
      close(stdoutfds[1]);
      close(stderrfds[1]);
    });

    doBoot();
    
    /* {
      std::cout << "init lib 3" << std::endl;
      void *libandroid_runtime_dso = dlopen("libandroid_runtime.so", RTLD_NOW);
      std::cout << "init lib 4 " << libandroid_runtime_dso << std::endl;
      void *sym = dlsym(libandroid_runtime_dso, "JNI_CreateJavaVM");
      std::cout << "init lib 5 " << sym << std::endl;
    }
    {
      std::cout << "init lib 6" << std::endl;
      void *libandroid_runtime_dso = dlopen("libnativehelper.so", RTLD_NOW);
      std::cout << "init lib 7 " << libandroid_runtime_dso << std::endl;
      void *sym = dlsym(libandroid_runtime_dso, "JNI_CreateJavaVM");
      std::cout << "init lib 8 " << sym << std::endl;
    } */

    const char *argsEnv = getenv("ARGS");
    if (argsEnv) {
      char args[4096];
      strncpy(args, argsEnv, sizeof(args));

      char *argv[64];
      size_t argc = 0;

      int argStartIndex = 0;
      for (int i = 0;; i++) {
        const char c = args[i];
        if (c == ' ') {
          args[i] = '\0';
          argv[argc] = args + argStartIndex;
          argc++;
          argStartIndex = i + 1;
          continue;
        } else if (c == '\0') {
          argv[argc] = args + argStartIndex;
          argc++;
          break;
        } else {
          continue;
        }
      }

      return node::Start(argc, argv);
    } else {
      const char *jsString;
      if (access("/package/app/index.html", F_OK) != -1) {
        jsString = "/package/app/index.html";
      } else {
        jsString = "examples/hello_ml.html";
      }
      
      const char *nodeString = "node";
      const char *dotString = ".";
      char argsString[4096];
      int i = 0;

      char *nodeArg = argsString + i;
      strncpy(nodeArg, nodeString, sizeof(argsString) - i);
      i += strlen(nodeString) + 1;

      char *dotArg = argsString + i;
      strncpy(dotArg, dotString, sizeof(argsString) - i);
      i += strlen(dotString) + 1;

      char *jsArg = argsString + i;
      strncpy(jsArg, jsString, sizeof(argsString) - i);
      i += strlen(jsString) + 1;

      char *argv[] = {nodeArg, dotArg, jsArg};
      size_t argc = sizeof(argv) / sizeof(argv[0]);

      return node::Start(argc, argv);
    }
  } else { // child
    ML_LOG_TAG(Info, LOG_TAG, "---------------------exokit start 1");

    close(stdoutfds[1]);
    close(stderrfds[1]);

    std::thread stdoutThread([]() -> void {
      int fd = stdoutfds[0];

      char buf[STDIO_BUF_SIZE + 1];
      ssize_t i = 0;
      ssize_t lineStart = 0;
      for (;;) {
        ssize_t size = read(fd, buf + i, STDIO_BUF_SIZE - i);
        // ML_LOG(Info, "=============read result %x %x %x", fd, size, errno);
        if (size > 0) {
          for (ssize_t j = i; j < i + size; j++) {
            if (buf[j] == '\n') {
              buf[j] = 0;
              ML_LOG_TAG(Info, LOG_TAG, "%s", buf + lineStart);

              lineStart = j + 1;
            }
          }

          i += size;

          if (i >= STDIO_BUF_SIZE) {
            ssize_t lineLength = i - lineStart;
            memcpy(buf, buf + lineStart, lineLength);
            i = lineLength;
            lineStart = 0;
          }
        } else {
          if (i > 0) {
            buf[i] = 0;
            ML_LOG_TAG(Info, LOG_TAG, "%s", buf);
          }
          break;
        }
      }
    });
    std::thread stderrThread([]() -> void {
      int fd = stderrfds[0];

      char buf[STDIO_BUF_SIZE + 1];
      ssize_t i = 0;
      ssize_t lineStart = 0;
      for (;;) {
        ssize_t size = read(fd, buf + i, STDIO_BUF_SIZE - i);
        // ML_LOG(Info, "=============read result %x %x %x", fd, size, errno);
        if (size > 0) {
          for (ssize_t j = i; j < i + size; j++) {
            if (buf[j] == '\n') {
              buf[j] = 0;
              ML_LOG_TAG(Info, LOG_TAG, "%s", buf + lineStart);

              lineStart = j + 1;
            }
          }

          i += size;

          if (i >= STDIO_BUF_SIZE) {
            ssize_t lineLength = i - lineStart;
            memcpy(buf, buf + lineStart, lineLength);
            i = lineLength;
            lineStart = 0;
          }
        } else {
          if (i > 0) {
            buf[i] = 0;
            ML_LOG_TAG(Info, LOG_TAG, "%s", buf);
          }
          break;
        }
      }
    });

    stdoutThread.join();
    stderrThread.join();

    ML_LOG_TAG(Info, LOG_TAG, "---------------------exokit end");

    return 0;
  }
}
