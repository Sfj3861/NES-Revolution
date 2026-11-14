package org.sfj3861.nesrevolution

internal object NativeBridge {
    init {
        System.loadLibrary("nesrev_android")
    }

    external fun nativeLoadRom(data: ByteArray): String?
    external fun nativeIsLoaded(): Boolean
    external fun nativeRunFrame(): IntArray?
    external fun nativeTakeAudioSamples(): FloatArray
    external fun nativeSetButton(buttonIndex: Int, pressed: Boolean)
    external fun nativeUnload()
}
