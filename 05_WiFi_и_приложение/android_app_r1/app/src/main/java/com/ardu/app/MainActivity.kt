package com.ardu.app

import android.app.Activity
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.TextView
import com.ardu.app.net.ArduApiClient
import com.ardu.app.net.LightStatus
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private val api = ArduApiClient()
    private val worker = Executors.newSingleThreadExecutor()

    private lateinit var connectionText: TextView
    private lateinit var statusText: TextView
    private lateinit var timeText: TextView
    private lateinit var refreshButton: Button

    private lateinit var lightStateText: TextView
    private lateinit var lightOnButton: Button
    private lateinit var lightOffButton: Button
    private lateinit var brightnessValueText: TextView
    private lateinit var brightnessSeekBar: SeekBar
    private lateinit var preset2700Button: Button
    private lateinit var preset4000Button: Button
    private lateinit var preset6000Button: Button
    private var lightApiAvailable = false

    private lateinit var developerToggleButton: Button
    private lateinit var developerPanel: LinearLayout
    private lateinit var rawCommandInput: EditText
    private lateinit var sendRawButton: Button
    private lateinit var rawResponseText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        connectionText = findViewById(R.id.connectionText)
        statusText = findViewById(R.id.statusText)
        timeText = findViewById(R.id.timeText)
        refreshButton = findViewById(R.id.refreshButton)

        lightStateText = findViewById(R.id.lightStateText)
        lightOnButton = findViewById(R.id.lightOnButton)
        lightOffButton = findViewById(R.id.lightOffButton)
        brightnessValueText = findViewById(R.id.brightnessValueText)
        brightnessSeekBar = findViewById(R.id.brightnessSeekBar)
        preset2700Button = findViewById(R.id.preset2700Button)
        preset4000Button = findViewById(R.id.preset4000Button)
        preset6000Button = findViewById(R.id.preset6000Button)

        developerToggleButton = findViewById(R.id.developerToggleButton)
        developerPanel = findViewById(R.id.developerPanel)
        rawCommandInput = findViewById(R.id.rawCommandInput)
        sendRawButton = findViewById(R.id.sendRawButton)
        rawResponseText = findViewById(R.id.rawResponseText)

        refreshButton.setOnClickListener { refreshDevice() }
        lightOnButton.setOnClickListener { setLightEnabled(true) }
        lightOffButton.setOnClickListener { setLightEnabled(false) }

        brightnessSeekBar.setOnSeekBarChangeListener(
            object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(
                    seekBar: SeekBar,
                    progress: Int,
                    fromUser: Boolean
                ) {
                    brightnessValueText.text = getString(
                        R.string.light_brightness_format,
                        progress
                    )
                }

                override fun onStartTrackingTouch(seekBar: SeekBar) = Unit

                override fun onStopTrackingTouch(seekBar: SeekBar) {
                    setLightBrightness(seekBar.progress)
                }
            }
        )

        preset2700Button.setOnClickListener { setLightKelvinPreset(2700) }
        preset4000Button.setOnClickListener { setLightKelvinPreset(4000) }
        preset6000Button.setOnClickListener { setLightKelvinPreset(6000) }

        developerToggleButton.setOnClickListener { toggleDeveloperMode() }
        sendRawButton.setOnClickListener { sendRawCommand() }

        refreshDevice()
    }

    override fun onDestroy() {
        worker.shutdownNow()
        super.onDestroy()
    }

    private fun refreshDevice() {
        setBusy(true)
        connectionText.text = getString(R.string.connection_checking)

        worker.execute {
            try {
                val ping = api.ping()
                val status = api.status()
                val time = api.time()
                val lightResult = runCatching { api.lightStatus() }

                runOnUiThread {
                    connectionText.text = "Онлайн • " + ping.firmware
                    statusText.text = status.nano.ifBlank { "—" }
                    timeText.text = time.nano
                        .removePrefix("TIME ")
                        .ifBlank { "—" }

                    lightResult
                        .onSuccess { updateLightStatus(it) }
                        .onFailure { showLightApiUnavailable() }

                    setBusy(false)
                }
            } catch (_: Exception) {
                runOnUiThread {
                    connectionText.text = getString(R.string.connection_offline)
                    statusText.text = "Не удалось получить состояние ARDU"
                    timeText.text = getString(R.string.unknown_value)
                    showLightApiUnavailable()
                    setBusy(false)
                }
            }
        }
    }

    private fun updateLightStatus(status: LightStatus) {
        lightApiAvailable = true

        val power = getString(
            if (status.enabled) R.string.light_power_on
            else R.string.light_power_off
        )

        val color = if (status.colorMode == "kelvin") {
            getString(R.string.light_color_kelvin, status.kelvin)
        } else {
            getString(
                R.string.light_color_rgb,
                status.red,
                status.green,
                status.blue
            )
        }

        lightStateText.text = getString(
            R.string.light_state_format,
            power,
            color,
            status.brightness
        )
        brightnessValueText.text = getString(
            R.string.light_brightness_format,
            status.brightness
        )
        brightnessSeekBar.progress = status.brightness
    }

    private fun showLightApiUnavailable() {
        lightApiAvailable = false
        lightStateText.text = getString(R.string.light_api_unavailable)
    }

    private fun setLightEnabled(enabled: Boolean) {
        applyLightChange { api.setLightEnabled(enabled) }
    }

    private fun setLightBrightness(brightness: Int) {
        applyLightChange { api.setLightBrightness(brightness) }
    }

    private fun setLightKelvinPreset(kelvin: Int) {
        applyLightChange { api.setLightKelvinPreset(kelvin) }
    }

    private fun applyLightChange(action: () -> Unit) {
        if (!lightApiAvailable) return

        setBusy(true)
        lightStateText.text = getString(R.string.light_applying)

        worker.execute {
            try {
                action()
                val light = api.lightStatus()
                val status = api.status()

                runOnUiThread {
                    statusText.text = status.nano.ifBlank { "—" }
                    updateLightStatus(light)
                    setBusy(false)
                }
            } catch (error: Exception) {
                runOnUiThread {
                    lightStateText.text = getString(
                        R.string.light_apply_error,
                        error.message ?: "UNKNOWN"
                    )
                    setBusy(false)
                }
            }
        }
    }

    private fun toggleDeveloperMode() {
        val willShow = developerPanel.visibility != View.VISIBLE
        developerPanel.visibility = if (willShow) View.VISIBLE else View.GONE
        developerToggleButton.text = getString(
            if (willShow) R.string.hide_developer_mode
            else R.string.developer_mode
        )
    }

    private fun sendRawCommand() {
        val command = rawCommandInput.text.toString()
        sendRawButton.isEnabled = false
        rawResponseText.text = "Отправка…"

        worker.execute {
            try {
                val response = api.sendNanoCommand(command)
                runOnUiThread {
                    rawResponseText.text = response.nano.ifBlank { "—" }
                    sendRawButton.isEnabled = true
                }
            } catch (error: Exception) {
                runOnUiThread {
                    rawResponseText.text = "ERR " + (error.message ?: "UNKNOWN")
                    sendRawButton.isEnabled = true
                }
            }
        }
    }

    private fun setBusy(busy: Boolean) {
        refreshButton.isEnabled = !busy
        lightOnButton.isEnabled = !busy && lightApiAvailable
        lightOffButton.isEnabled = !busy && lightApiAvailable
        brightnessSeekBar.isEnabled = !busy && lightApiAvailable
        preset2700Button.isEnabled = !busy && lightApiAvailable
        preset4000Button.isEnabled = !busy && lightApiAvailable
        preset6000Button.isEnabled = !busy && lightApiAvailable
    }
}
