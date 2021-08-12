#include <jni.h>
#include <android_native_app_glue.h>
#include <string>
#include <SDL.h>

std::string getFilePath()
{
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass cls = env->GetObjectClass(activity);
    //jclass localcls = reinterpret_cast<jclass>(env->NewGlobalRef(cls));
    jmethodID getFilePath = env->GetMethodID(cls, "getFilePath", "()Ljava/lang/String;");
    jstring str = (jstring)env->CallObjectMethod(activity, getFilePath);
    const char *str2 = env->GetStringUTFChars(str, 0);
    return std::string(str2);
}

std::string getDirPath()
{
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass cls = env->GetObjectClass(activity);
    //jclass localcls = reinterpret_cast<jclass>(env->NewGlobalRef(cls));
    jmethodID getFilePath = env->GetMethodID(cls, "getFilePath1", "()Ljava/lang/String;");
    jstring str = (jstring)env->CallObjectMethod(activity, getFilePath);
    const char *str2 = env->GetStringUTFChars(str, 0);
    return std::string(str2);
}

bool rtlsdr_device_android_ready = false;
int rtlsdr_device_android_fd = 0;
std::string rtlsdr_device_android_path = "";

extern "C" JNIEXPORT jboolean JNICALL
Java_com_altillimity_satdump_SdrDevice_doOpenRTL(JNIEnv *env, jclass, jint fd, jstring path)
{
    rtlsdr_device_android_fd = fd;
    rtlsdr_device_android_path = std::string(env->GetStringUTFChars(path, 0));
    rtlsdr_device_android_ready = true;

    return false;
}

bool airspy_device_android_ready = false;
int airspy_device_android_fd = 0;
std::string airspy_device_android_path = "";

extern "C" JNIEXPORT jboolean JNICALL
Java_com_altillimity_satdump_SdrDevice_doOpenAirspy(JNIEnv *env, jclass, jint fd, jstring path)
{
    airspy_device_android_fd = fd;
    airspy_device_android_path = std::string(env->GetStringUTFChars(path, 0));
    airspy_device_android_ready = true;

    return false;
}

bool airspyhf_device_android_ready = false;
int airspyhf_device_android_fd = 0;
std::string airspyhf_device_android_path = "";

extern "C" JNIEXPORT jboolean JNICALL
Java_com_altillimity_satdump_SdrDevice_doOpenAirspyHF(JNIEnv *env, jclass, jint fd, jstring path)
{
    airspyhf_device_android_fd = fd;
    airspyhf_device_android_path = std::string(env->GetStringUTFChars(path, 0));
    airspyhf_device_android_ready = true;

    return false;
}