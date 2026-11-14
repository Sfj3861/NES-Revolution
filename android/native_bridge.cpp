#include <jni.h>

#include "nesrev/nes_machine.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace {
std::mutex g_mutex;
std::unique_ptr<nesrev::NesMachine> g_machine = std::make_unique<nesrev::NesMachine>();
nesrev::ControllerState g_controller1;
std::uint64_t g_input_sequence = 0;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_sfj3861_nesrevolution_NativeBridge_nativeLoadRom(
    JNIEnv* env, jobject, jbyteArray rom_data) {
    if (rom_data == nullptr) return env->NewStringUTF("No ROM data was supplied");
    const auto length = env->GetArrayLength(rom_data);
    if (length <= 0) return env->NewStringUTF("The selected file is empty");

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(rom_data, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return env->NewStringUTF("Could not read the selected ROM data");
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_machine->load_rom(std::span<const std::uint8_t>(bytes.data(), bytes.size()))) {
        return env->NewStringUTF(g_machine->error().c_str());
    }
    g_controller1 = nesrev::ControllerState{};
    g_machine->set_controller1(g_controller1);
    return nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_sfj3861_nesrevolution_NativeBridge_nativeIsLoaded(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_machine->loaded() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_org_sfj3861_nesrevolution_NativeBridge_nativeRunFrame(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_machine->loaded() || !g_machine->run_frame()) return nullptr;

    const auto& frame = g_machine->framebuffer();
    jintArray result = env->NewIntArray(static_cast<jsize>(frame.size()));
    if (result == nullptr) return nullptr;

    std::vector<jint> pixels;
    pixels.reserve(frame.size());
    for (const auto pixel : frame) pixels.push_back(static_cast<jint>(pixel));
    env->SetIntArrayRegion(result, 0, static_cast<jsize>(pixels.size()), pixels.data());
    return result;
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_org_sfj3861_nesrevolution_NativeBridge_nativeTakeAudioSamples(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto samples = g_machine->take_audio_samples();
    jfloatArray result = env->NewFloatArray(static_cast<jsize>(samples.size()));
    if (result == nullptr || samples.empty()) return result;
    env->SetFloatArrayRegion(result, 0, static_cast<jsize>(samples.size()), samples.data());
    return result;
}

extern "C" JNIEXPORT void JNICALL
Java_org_sfj3861_nesrevolution_NativeBridge_nativeSetButton(
    JNIEnv*, jobject, jint button_index, jboolean pressed) {
    if (button_index < 0 || button_index >= static_cast<jint>(nesrev::Button::Count)) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    const auto button = static_cast<nesrev::Button>(button_index);
    const auto event = nesrev::InputEvent{button, pressed == JNI_TRUE, 0, ++g_input_sequence};
    g_controller1.apply(event);
    g_machine->set_controller1(g_controller1);
}

extern "C" JNIEXPORT void JNICALL
Java_org_sfj3861_nesrevolution_NativeBridge_nativeUnload(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_machine->unload();
    g_controller1 = nesrev::ControllerState{};
    g_machine->set_controller1(g_controller1);
}
