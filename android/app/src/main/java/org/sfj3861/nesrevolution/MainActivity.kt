package org.sfj3861.nesrevolution

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.ActivityInfo
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import android.os.Bundle
import android.os.Handler
import android.os.HandlerThread
import android.view.KeyEvent
import android.view.WindowManager
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import java.io.IOException

class MainActivity : Activity() {
    private lateinit var status: TextView
    private lateinit var gameScreen: GameScreenView
    private lateinit var emulationHandler: Handler
    private lateinit var emulationThread: HandlerThread
    private var audioTrack: AudioTrack? = null
    private var loopRunning = false
    private val keyPreferences by lazy { getSharedPreferences("controller_keys", MODE_PRIVATE) }
    private val buttonKeys by lazy { IntArray(8) { keyPreferences.getInt("key_$it", DEFAULT_KEYS[it]) } }

    private var selectedButtonIndex = 0
    private var capturingKeyboardKey = false
    private var keyDialog: AlertDialog? = null
    private var consoleButton: Button? = null
    private var keyboardButton: Button? = null

    private val frameLoop = object : Runnable {
        override fun run() {
            if (!loopRunning) return
            val start = System.nanoTime()
            val frame = NativeBridge.nativeRunFrame()
            if (frame != null) {
                runOnUiThread { gameScreen.setFrame(frame) }
                val samples = NativeBridge.nativeTakeAudioSamples()
                if (samples.isNotEmpty()) {
                    audioTrack?.write(samples, 0, samples.size, AudioTrack.WRITE_BLOCKING)
                }
            } else if (!NativeBridge.nativeIsLoaded()) {
                runOnUiThread { status.text = "La ROM se ha detenido. Prueba otro juego NROM." }
                loopRunning = false
                return
            }

            val elapsedMs = (System.nanoTime() - start) / 1_000_000L
            emulationHandler.postDelayed(this, (16L - elapsedMs).coerceAtLeast(1L))
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF101216.toInt())
        }

        val top = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(dp(12), dp(6), dp(12), dp(6))
            gravity = android.view.Gravity.CENTER_VERTICAL
        }
        val title = TextView(this).apply {
            text = "NES Revolution"
            textSize = 18f
            setTextColor(0xFFFFFFFF.toInt())
        }
        status = TextView(this).apply {
            text = "Carga una ROM .nes para empezar"
            textSize = 12f
            setTextColor(0xFFB9C0CA.toInt())
            setPadding(dp(12), 0, dp(12), 0)
        }
        val openButton = Button(this).apply {
            text = "Abrir ROM"
            setOnClickListener { openRomPicker() }
        }
        top.addView(title)
        top.addView(status, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        top.addView(openButton)
        val configureKeysButton = Button(this).apply {
            text = "Teclas"
            setOnClickListener { showKeyConfiguration() }
        }
        top.addView(configureKeysButton)
        root.addView(top, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
        ))

        gameScreen = GameScreenView(this)
        root.addView(gameScreen, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f
        ))

        val controls = ControllerView(this).apply {
            onButtonsChanged = { buttons ->
                for (index in 0 until 8) {
                    NativeBridge.nativeSetButton(index, buttons.contains(index))
                }
            }
            contentDescription = "Controles táctiles de NES"
        }
        root.addView(controls, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, dp(176)
        ))

        setContentView(root)
        emulationThread = HandlerThread("NES-Emulation").apply { start() }
        emulationHandler = Handler(emulationThread.looper)
    }

    private fun openRomPicker() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        @Suppress("DEPRECATION")
        startActivityForResult(intent, REQUEST_OPEN_ROM)
    }

    @Deprecated("Uses ACTION_OPEN_DOCUMENT for broad Android compatibility")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQUEST_OPEN_ROM || resultCode != RESULT_OK) return
        val uri = data?.data ?: return

        status.text = "Cargando ROM..."
        try {
            val bytes = contentResolver.openInputStream(uri)?.use { it.readBytes() }
                ?: throw IOException("No se pudo abrir el archivo")
            val error = NativeBridge.nativeLoadRom(bytes)
            if (error != null) {
                status.text = error
                stopEmulation()
                return
            }
            status.text = "ROM cargada · NROM / Mapper 0"
            createAudioOutput()
            startEmulation()
        } catch (exception: Exception) {
            status.text = "Error al abrir la ROM: " + (exception.message ?: "desconocido")
        }
    }

    private fun createAudioOutput() {
        if (audioTrack != null) return
        val minimum = AudioTrack.getMinBufferSize(
            SAMPLE_RATE, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_FLOAT
        )
        if (minimum <= 0) {
            status.text = "ROM cargada · audio no disponible"
            return
        }
        val format = AudioFormat.Builder()
            .setSampleRate(SAMPLE_RATE)
            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
            .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
            .build()
        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build()
        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(attributes)
            .setAudioFormat(format)
            .setBufferSizeInBytes(maxOf(minimum, 8192))
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build().also { it.play() }
    }

    private fun startEmulation() {
        if (loopRunning) return
        loopRunning = true
        emulationHandler.post(frameLoop)
    }

    private fun stopEmulation() {
        loopRunning = false
        if (::emulationHandler.isInitialized) emulationHandler.removeCallbacks(frameLoop)
    }

    override fun onResume() {
        super.onResume()
        if (::emulationHandler.isInitialized && NativeBridge.nativeIsLoaded()) {
            startEmulation()
            audioTrack?.play()
        }
    }

    override fun onPause() {
        stopEmulation()
        audioTrack?.pause()
        super.onPause()
    }

    private fun showKeyConfiguration() {
        selectedButtonIndex = 0
        capturingKeyboardKey = false

        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(12), dp(20), dp(4))
        }
        val explanation = TextView(this).apply {
            text = "Elige primero el botón de la NES y después la tecla del teclado que quieres usar para pulsarlo. Por ejemplo: A = Espacio."
            textSize = 14f
            setTextColor(0xFF202124.toInt())
            setPadding(0, 0, 0, dp(12))
        }
        consoleButton = Button(this).apply {
            text = "Botón de NES: ${BUTTON_NAMES[selectedButtonIndex]}"
            setOnClickListener {
                AlertDialog.Builder(this@MainActivity)
                    .setTitle("¿Qué botón de la NES quieres asignar?")
                    .setItems(BUTTON_NAMES) { _, index ->
                        selectedButtonIndex = index
                        text = "Botón de NES: ${BUTTON_NAMES[index]}"
                        keyboardButton?.text =
                            "Tecla del teclado: ${KeyEvent.keyCodeToString(buttonKeys[index])}"
                    }
                    .setNegativeButton("Cancelar", null)
                    .show()
            }
        }
        keyboardButton = Button(this).apply {
            text = "Tecla del teclado: ${KeyEvent.keyCodeToString(buttonKeys[selectedButtonIndex])}"
            setOnClickListener {
                capturingKeyboardKey = true
                status.text = "Pulsa ahora la tecla que quieres asignar a ${BUTTON_NAMES[selectedButtonIndex]}."
            }
        }
        panel.addView(explanation)
        panel.addView(consoleButton)
        panel.addView(keyboardButton)

        keyDialog = AlertDialog.Builder(this)
            .setTitle("Configurar controles")
            .setView(panel)
            .setNegativeButton("Cerrar", null)
            .create()
        keyDialog?.setOnDismissListener {
            capturingKeyboardKey = false
            consoleButton = null
            keyboardButton = null
        }
        keyDialog?.show()
    }

    private fun captureConfiguredKey(keyCode: Int): Boolean {
        if (!capturingKeyboardKey) return false
        if (keyCode == KeyEvent.KEYCODE_BACK) return false

        val alreadyUsedBy = buttonKeys.indices.firstOrNull {
            it != selectedButtonIndex && buttonKeys[it] == keyCode
        }
        if (alreadyUsedBy != null) {
            status.text = "Esa tecla ya está asignada a ${BUTTON_NAMES[alreadyUsedBy]}. Elige otra."
            return true
        }

        buttonKeys[selectedButtonIndex] = keyCode
        keyPreferences.edit().putInt("key_$selectedButtonIndex", keyCode).apply()
        keyboardButton?.text = "Tecla del teclado: ${KeyEvent.keyCodeToString(keyCode)}"
        capturingKeyboardKey = false
        status.text = "${BUTTON_NAMES[selectedButtonIndex]} ahora se activa con ${KeyEvent.keyCodeToString(keyCode)}."
        return true
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (captureConfiguredKey(keyCode)) return true
        val button = keyToButton(keyCode) ?: return super.onKeyDown(keyCode, event)
        NativeBridge.nativeSetButton(button, true)
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        val button = keyToButton(keyCode) ?: return super.onKeyUp(keyCode, event)
        NativeBridge.nativeSetButton(button, false)
        return true
    }

    private fun keyToButton(keyCode: Int): Int? = buttonKeys.indexOf(keyCode).takeIf { it >= 0 }

    override fun onDestroy() {
        capturingKeyboardKey = false
        keyDialog?.dismiss()
        stopEmulation()
        if (::emulationThread.isInitialized) emulationThread.quitSafely()
        audioTrack?.stop()
        audioTrack?.release()
        audioTrack = null
        NativeBridge.nativeUnload()
        super.onDestroy()
    }

    private fun dp(value: Int): Int = (value * resources.displayMetrics.density).toInt()

    companion object {
        private const val REQUEST_OPEN_ROM = 4201
        private const val SAMPLE_RATE = 48000
        private val BUTTON_NAMES = arrayOf("A", "B", "Select", "Start", "Arriba", "Abajo", "Izquierda", "Derecha")
        private val DEFAULT_KEYS = intArrayOf(
            KeyEvent.KEYCODE_Z, KeyEvent.KEYCODE_X, KeyEvent.KEYCODE_SHIFT_LEFT,
            KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN,
            KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT
        )
    }
}
