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
    private lateinit var kelvinValueText: TextView
    private lateinit var kelvinSeekBar: SeekBar
    private lateinit var redValueText: TextView
    private lateinit var greenValueText: TextView
    private lateinit var blueValueText: TextView
    private lateinit var redSeekBar: SeekBar
    private lateinit var greenSeekBar: SeekBar
    private lateinit var blueSeekBar: SeekBar
    private lateinit var applyRgbButton: Button
    private lateinit var profileStateText: TextView
    private lateinit var saveStartupProfileButton: Button
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
        kelvinValueText = findViewById(R.id.kelvinValueText)
        kelvinSeekBar = findViewById(R.id.kelvinSeekBar)
        redValueText = findViewById(R.id.redValueText)
        greenValueText = findViewById(R.id.greenValueText)
        blueValueText = findViewById(R.id.blueValueText)
        redSeekBar = findViewById(R.id.redSeekBar)
        greenSeekBar = findViewById(R.id.greenSeekBar)
        blueSeekBar = findViewById(R.id.blueSeekBar)
        applyRgbButton = findViewById(R.id.applyRgbButton)
        profileStateText = findViewById(R.id.profileStateText)
        saveStartupProfileButton = findViewById(R.id.saveStartupProfileButton)

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

        preset2700Button.setOnClickListener { setLightKelvin(2700) }
        preset4000Button.setOnClickListener { setLightKelvin(4000) }
        preset6000Button.setOnClickListener { setLightKelvin(6000) }

        kelvinSeekBar.setOnSeekBarChangeListener(
            object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(
                    seekBar: SeekBar,
                    progress: Int,
                    fromUser: Boolean
                ) {
                    kelvinValueText.text = getString(
                        R.string.light_kelvin_format,
                        progress
                    )
                }

                override fun onStartTrackingTouch(seekBar: SeekBar) = Unit

                override fun onStopTrackingTouch(seekBar: SeekBar) {
                    setLightKelvin(seekBar.progress)
                }
            }
        )

        val rgbListener = object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(
                seekBar: SeekBar,
                progress: Int,
                fromUser: Boolean
            ) {
                updateRgbLabels()
            }

            override fun onStartTrackingTouch(seekBar: SeekBar) = Unit
            override fun onStopTrackingTouch(seekBar: SeekBar) = Unit
        }

        redSeekBar.setOnSeekBarChangeListener(rgbListener)
        greenSeekBar.setOnSeekBarChangeListener(rgbListener)
        blueSeekBar.setOnSeekBarChangeListener(rgbListener)

        applyRgbButton.setOnClickListener {
            setLightRgb(
                redSeekBar.progress,
                greenSeekBar.progress,
                blueSeekBar.progress
            )
        }

        saveStartupProfileButton.setOnClickListener {
            saveLightStartupProfile()
        }

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
        kelvinValueText.text = getString(
            R.string.light_kelvin_format,
            status.kelvin
        )
        kelvinSeekBar.progress = status.kelvin
        redSeekBar.progress = status.red
        greenSeekBar.progress = status.green
        blueSeekBar.progress = status.blue
        updateRgbLabels()

        profileStateText.text = getString(
            if (status.dirty) R.string.light_profile_unsaved
            else R.string.light_profile_saved
        )
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

    private fun setLightKelvin(kelvin: Int) {
        applyLightChange { api.setLightKelvin(kelvin) }
    }

    private fun setLightRgb(red: Int, green: Int, blue: Int) {
        applyLightChange { api.setLightRgb(red, green, blue) }
    }

    private fun saveLightStartupProfile() {
        applyLightChange { api.saveLightStartupProfile() }
    }

    private fun updateRgbLabels() {
        redValueText.text = getString(
            R.string.light_rgb_component_format,
            "R",
            redSeekBar.progress
        )
        greenValueText.text = getString(
            R.string.light_rgb_component_format,
            "G",
            greenSeekBar.progress
        )
        blueValueText.text = getString(
            R.string.light_rgb_component_format,
            "B",
            blueSeekBar.progress
        )
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
        kelvinSeekBar.isEnabled = !busy && lightApiAvailable
        redSeekBar.isEnabled = !busy && lightApiAvailable
        greenSeekBar.isEnabled = !busy && lightApiAvailable
        blueSeekBar.isEnabled = !busy && lightApiAvailable
        applyRgbButton.isEnabled = !busy && lightApiAvailable
        saveStartupProfileButton.isEnabled = !busy && lightApiAvailable
    }
}
