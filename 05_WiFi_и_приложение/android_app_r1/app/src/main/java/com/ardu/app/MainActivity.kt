package com.ardu.app

import android.app.Activity
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
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

        developerToggleButton = findViewById(R.id.developerToggleButton)
        developerPanel = findViewById(R.id.developerPanel)
        rawCommandInput = findViewById(R.id.rawCommandInput)
        sendRawButton = findViewById(R.id.sendRawButton)
        rawResponseText = findViewById(R.id.rawResponseText)

        refreshButton.setOnClickListener { refreshDevice() }
        lightOnButton.setOnClickListener { setLightEnabled(true) }
        lightOffButton.setOnClickListener { setLightEnabled(false) }
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
    }

    private fun showLightApiUnavailable() {
        lightApiAvailable = false
        lightStateText.text = getString(R.string.light_api_unavailable)
    }

    private fun setLightEnabled(enabled: Boolean) {
        if (!lightApiAvailable) return

        setBusy(true)
        lightStateText.text = getString(R.string.light_applying)

        worker.execute {
            try {
                api.setLightEnabled(enabled)
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
    }
}
