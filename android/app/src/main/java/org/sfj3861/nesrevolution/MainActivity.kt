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
import android.widget.ScrollView
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
    private var keyDialog: AlertDialog? = null
    private var keyPickerDialog: AlertDialog? = null
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
        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(12), dp(20), dp(4))
        }
        val explanation = TextView(this).apply {
            text = "Selecciona un botón de la NES y asígnale una tecla del teclado virtual. Por ejemplo: A = ESPACIO."
            textSize = 14f
            setTextColor(0xFF202124.toInt())
            setPadding(0, 0, 0, dp(12))
        }
        consoleButton = Button(this).apply {
            text = "Botón de NES: ${BUTTON_NAMES[selectedButtonIndex]}"
            setOnClickListener {
                AlertDialog.Builder(this@MainActivity)
                    .setTitle("¿Qué botón de la NES quieres configurar?")
                    .setItems(BUTTON_NAMES) { _, index ->
                        selectedButtonIndex = index
                        text = "Botón de NES: ${BUTTON_NAMES[index]}"
                        keyboardButton?.text = "Tecla asignada: ${keyLabel(buttonKeys[index])}"
                    }
                    .setNegativeButton("Cancelar", null)
                    .show()
            }
        }
        keyboardButton = Button(this).apply {
            text = "Tecla asignada: ${keyLabel(buttonKeys[selectedButtonIndex])}"
            setOnClickListener { showVirtualKeyboard() }
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
            keyDialog = null
            consoleButton = null
            keyboardButton = null
            keyPickerDialog?.dismiss()
            keyPickerDialog = null
        }
        keyDialog?.show()
    }

    private fun showVirtualKeyboard() {
        val scroll = ScrollView(this)
        val keyboard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(8), dp(8), dp(8), dp(8))
        }
        scroll.addView(keyboard)
        val rows = listOf(
            listOf("1" to KeyEvent.KEYCODE_1, "2" to KeyEvent.KEYCODE_2, "3" to KeyEvent.KEYCODE_3, "4" to KeyEvent.KEYCODE_4, "5" to KeyEvent.KEYCODE_5, "6" to KeyEvent.KEYCODE_6, "7" to KeyEvent.KEYCODE_7, "8" to KeyEvent.KEYCODE_8, "9" to KeyEvent.KEYCODE_9, "0" to KeyEvent.KEYCODE_0),
            listOf("Q" to KeyEvent.KEYCODE_Q, "W" to KeyEvent.KEYCODE_W, "E" to KeyEvent.KEYCODE_E, "R" to KeyEvent.KEYCODE_R, "T" to KeyEvent.KEYCODE_T, "Y" to KeyEvent.KEYCODE_Y, "U" to KeyEvent.KEYCODE_U, "I" to KeyEvent.KEYCODE_I, "O" to KeyEvent.KEYCODE_O, "P" to KeyEvent.KEYCODE_P),
            listOf("A" to KeyEvent.KEYCODE_A, "S" to KeyEvent.KEYCODE_S, "D" to KeyEvent.KEYCODE_D, "F" to KeyEvent.KEYCODE_F, "G" to KeyEvent.KEYCODE_G, "H" to KeyEvent.KEYCODE_H, "J" to KeyEvent.KEYCODE_J, "K" to KeyEvent.KEYCODE_K, "L" to KeyEvent.KEYCODE_L),
            listOf("Z" to KeyEvent.KEYCODE_Z, "X" to KeyEvent.KEYCODE_X, "C" to KeyEvent.KEYCODE_C, "V" to KeyEvent.KEYCODE_V, "B" to KeyEvent.KEYCODE_B, "N" to KeyEvent.KEYCODE_N, "M" to KeyEvent.KEYCODE_M),
            listOf("SHIFT" to KeyEvent.KEYCODE_SHIFT_LEFT, "CTRL" to KeyEvent.KEYCODE_CTRL_LEFT, "ALT" to KeyEvent.KEYCODE_ALT_LEFT, "TAB" to KeyEvent.KEYCODE_TAB, "⌫" to KeyEvent.KEYCODE_DEL),
            listOf("←" to KeyEvent.KEYCODE_DPAD_LEFT, "↑" to KeyEvent.KEYCODE_DPAD_UP, "↓" to KeyEvent.KEYCODE_DPAD_DOWN, "→" to KeyEvent.KEYCODE_DPAD_RIGHT, "ENTER" to KeyEvent.KEYCODE_ENTER, "ESC" to KeyEvent.KEYCODE_ESCAPE)
        )
        rows.forEach { row ->
            val line = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
            row.forEach { (label, code) ->
                val key = Button(this).apply {
                    text = label
                    textSize = 11f
                    setPadding(dp(2), 0, dp(2), 0)
                    setOnClickListener { assignVirtualKey(code) }
                }
                line.addView(key, LinearLayout.LayoutParams(0, dp(48), 1f).apply {
                    setMargins(dp(2), dp(2), dp(2), dp(2))
                })
            }
            keyboard.addView(line, LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
            ))
        }
        val space = Button(this).apply {
            text = "ESPACIO"
            setOnClickListener { assignVirtualKey(KeyEvent.KEYCODE_SPACE) }
        }
        keyboard.addView(space, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, dp(52)
        ).apply { setMargins(dp(2), dp(4), dp(2), dp(2)) })

        keyPickerDialog = AlertDialog.Builder(this)
            .setTitle("Elige la tecla para ${BUTTON_NAMES[selectedButtonIndex]}")
            .setView(scroll)
            .setNegativeButton("Cancelar", null)
            .create()
        keyPickerDialog?.setOnDismissListener { keyPickerDialog = null }
        keyPickerDialog?.show()
    }

    private fun assignVirtualKey(keyCode: Int) {
        val alreadyUsedBy = buttonKeys.indices.firstOrNull {
            it != selectedButtonIndex && buttonKeys[it] == keyCode
        }
        if (alreadyUsedBy != null) {
            status.text = "Esa tecla ya está asignada a ${BUTTON_NAMES[alreadyUsedBy]}. Elige otra."
            return
        }
        buttonKeys[selectedButtonIndex] = keyCode
        keyPreferences.edit().putInt("key_$selectedButtonIndex", keyCode).apply()
        keyboardButton?.text = "Tecla asignada: ${keyLabel(keyCode)}"
        status.text = "${BUTTON_NAMES[selectedButtonIndex]} ahora se activa con ${keyLabel(keyCode)}."
        keyPickerDialog?.dismiss()
    }

    private fun keyLabel(keyCode: Int): String = when (keyCode) {
        KeyEvent.KEYCODE_SPACE -> "ESPACIO"
        KeyEvent.KEYCODE_SHIFT_LEFT -> "SHIFT"
        KeyEvent.KEYCODE_CTRL_LEFT -> "CTRL"
        KeyEvent.KEYCODE_ALT_LEFT -> "ALT"
        KeyEvent.KEYCODE_DPAD_UP -> "FLECHA ARRIBA"
        KeyEvent.KEYCODE_DPAD_DOWN -> "FLECHA ABAJO"
        KeyEvent.KEYCODE_DPAD_LEFT -> "FLECHA IZQUIERDA"
        KeyEvent.KEYCODE_DPAD_RIGHT -> "FLECHA DERECHA"
        KeyEvent.KEYCODE_DEL -> "RETROCESO"
        KeyEvent.KEYCODE_ESCAPE -> "ESC"
        else -> KeyEvent.keyCodeToString(keyCode).removePrefix("KEYCODE_")
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
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
        keyPickerDialog?.dismiss()
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
