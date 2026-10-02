package com.ardu.app

import android.app.Activity
import android.os.Bundle
import android.graphics.Color
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.Switch
import android.widget.TextView
import com.ardu.app.net.ArduApiClient
import com.ardu.app.net.ArduSettings
import com.ardu.app.net.DeviceStatus
import com.ardu.app.net.TimeStatus
import com.ardu.app.ui.ColorWheelView
import com.ardu.app.ui.SmartSliderView
import java.util.Locale
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private val api = ArduApiClient()
    private val worker = Executors.newSingleThreadExecutor()
    private var rendering = false
    private var latestSettings: ArduSettings? = null
    private var sectionNavigation: List<Pair<Button, LinearLayout>> = emptyList()
    private var lastEspFirmware: String = "—"
    private var lastEspRssi: Int? = null

    private lateinit var connectionText: TextView
    private lateinit var modeText: TextView
    private lateinit var timeText: TextView
    private lateinit var operationText: TextView
    private lateinit var refreshButton: Button

    private lateinit var lightPanel: LinearLayout
    private lateinit var musicPanel: LinearLayout
    private lateinit var ambientPanel: LinearLayout
    private lateinit var nightPanel: LinearLayout
    private lateinit var alarmPanel: LinearLayout
    private lateinit var servicePanel: LinearLayout

    private lateinit var lightStateText: TextView
    private lateinit var lightBrightnessText: TextView
    private lateinit var lightBrightnessSeek: SmartSliderView
    private lateinit var lightKelvinText: TextView
    private lateinit var lightKelvinSeek: SmartSliderView
    private lateinit var lightRgbText: TextView
    private lateinit var lightRedSeek: SeekBar
    private lateinit var lightGreenSeek: SeekBar
    private lateinit var lightBlueSeek: SeekBar
    private lateinit var lightColorPreview: View
    private lateinit var lightColorWheel: ColorWheelView
    private lateinit var lightAdvancedRgbPanel: LinearLayout
    private lateinit var clapEnabledSwitch: Switch
    private lateinit var clapStateText: TextView
    private lateinit var clapCalibrationPanel: LinearLayout
    private lateinit var clapCalibrationText: TextView
    private lateinit var clapSampleButton: Button
    private lateinit var clapFinishButton: Button
    private lateinit var clapSaveButton: Button

    private lateinit var musicModeText: TextView
    private lateinit var musicModeButtons: LinearLayout
    private lateinit var musicBrightnessText: TextView
    private lateinit var musicBrightnessSeek: SeekBar
    private lateinit var musicBackgroundText: TextView
    private lateinit var musicBackgroundSeek: SeekBar
    private lateinit var musicSmoothingGroup: LinearLayout
    private lateinit var musicSmoothingText: TextView
    private lateinit var musicSmoothingSeek: SeekBar
    private lateinit var musicSensitivityGroup: LinearLayout
    private lateinit var musicSensitivityText: TextView
    private lateinit var musicSensitivitySeek: SeekBar
    private lateinit var musicSubmodeGroup: LinearLayout
    private lateinit var musicSpeedGroup: LinearLayout
    private lateinit var musicSpeedText: TextView
    private lateinit var musicSpeedSeek: SeekBar
    private lateinit var musicAuxGroup: LinearLayout
    private lateinit var musicAuxText: TextView
    private lateinit var musicAuxSeek: SeekBar
    private lateinit var musicHueStartGroup: LinearLayout
    private lateinit var musicHueStartText: TextView
    private lateinit var musicHueStartSeek: SeekBar
    private lateinit var musicCalibrationText: TextView

    private lateinit var ambientEffectText: TextView
    private lateinit var ambientColorPreview: View
    private lateinit var ambientHueText: TextView
    private lateinit var ambientHueSeek: SeekBar
    private lateinit var ambientSaturationGroup: LinearLayout
    private lateinit var ambientSaturationText: TextView
    private lateinit var ambientSaturationSeek: SeekBar
    private lateinit var ambientBrightnessText: TextView
    private lateinit var ambientBrightnessSeek: SeekBar
    private lateinit var ambientSpeedGroup: LinearLayout
    private lateinit var ambientSpeedText: TextView
    private lateinit var ambientSpeedSeek: SeekBar
    private lateinit var ambientRainbowGroup: LinearLayout
    private lateinit var ambientRainbowText: TextView
    private lateinit var ambientRainbowSeek: SeekBar
    private lateinit var ambientAutoSwitch: Switch
    private lateinit var ambientPeriodText: TextView
    private lateinit var ambientPeriodSeek: SeekBar

    private lateinit var nightEnabledSwitch: Switch
    private lateinit var nightColorPreview: View
    private lateinit var nightHueText: TextView
    private lateinit var nightHueSeek: SeekBar
    private lateinit var nightSaturationText: TextView
    private lateinit var nightSaturationSeek: SeekBar
    private lateinit var nightBrightnessText: TextView
    private lateinit var nightBrightnessSeek: SeekBar
    private lateinit var nightScheduleSwitch: Switch
    private lateinit var nightOnInput: EditText
    private lateinit var nightOffInput: EditText

    private lateinit var alarmStateText: TextView
    private lateinit var alarmStartPreview: View
    private lateinit var alarmEndPreview: View
    private lateinit var alarmEnabledSwitch: Switch
    private lateinit var alarmHourInput: EditText
    private lateinit var alarmMinuteInput: EditText
    private lateinit var alarmFadeText: TextView
    private lateinit var alarmFadeSeek: SeekBar
    private lateinit var alarmBrightnessText: TextView
    private lateinit var alarmBrightnessSeek: SeekBar
    private lateinit var alarmStartHueText: TextView
    private lateinit var alarmStartHueSeek: SeekBar
    private lateinit var alarmEndHueText: TextView
    private lateinit var alarmEndHueSeek: SeekBar

    private lateinit var addressInput: EditText
    private lateinit var systemSummaryText: TextView
    private lateinit var currentLimitText: TextView
    private lateinit var currentLimitSeek: SeekBar
    private lateinit var eventsText: TextView
    private lateinit var developerPanel: LinearLayout
    private lateinit var developerToggleButton: Button
    private lateinit var rawCommandInput: EditText
    private lateinit var rawResponseText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bindViews()
        bindNavigation()
        bindLight()
        bindMusic()
        bindAmbient()
        bindNight()
        bindAlarm()
        bindService()

        val savedAddress = preferences().getString(PREF_ADDRESS, null)
        if (!savedAddress.isNullOrBlank()) {
            api.setPreferredAddress(savedAddress)
            addressInput.setText(savedAddress.removePrefix("http://"))
        }

        showSection(lightPanel)
        refreshDevice()
    }

    override fun onDestroy() {
        worker.shutdownNow()
        super.onDestroy()
    }

    private fun bindViews() {
        connectionText = findViewById(R.id.connectionText)
        modeText = findViewById(R.id.modeText)
        timeText = findViewById(R.id.timeText)
        operationText = findViewById(R.id.operationText)
        refreshButton = findViewById(R.id.refreshButton)

        lightPanel = findViewById(R.id.lightPanel)
        musicPanel = findViewById(R.id.musicPanel)
        ambientPanel = findViewById(R.id.ambientPanel)
        nightPanel = findViewById(R.id.nightPanel)
        alarmPanel = findViewById(R.id.alarmPanel)
        servicePanel = findViewById(R.id.servicePanel)

        lightStateText = findViewById(R.id.lightStateText)
        lightBrightnessText = findViewById(R.id.lightBrightnessText)
        lightBrightnessSeek = findViewById(R.id.lightBrightnessSeek)
        lightKelvinText = findViewById(R.id.lightKelvinText)
        lightKelvinSeek = findViewById(R.id.lightKelvinSeek)
        lightRgbText = findViewById(R.id.lightRgbText)
        lightRedSeek = findViewById(R.id.lightRedSeek)
        lightGreenSeek = findViewById(R.id.lightGreenSeek)
        lightBlueSeek = findViewById(R.id.lightBlueSeek)
        lightColorPreview = findViewById(R.id.lightColorPreview)
        lightColorWheel = findViewById(R.id.lightColorWheel)
        lightAdvancedRgbPanel = findViewById(R.id.lightAdvancedRgbPanel)
        clapEnabledSwitch = findViewById(R.id.clapEnabledSwitch)
        clapStateText = findViewById(R.id.clapStateText)
        clapCalibrationPanel = findViewById(R.id.clapCalibrationPanel)
        clapCalibrationText = findViewById(R.id.clapCalibrationText)
        clapSampleButton = findViewById(R.id.clapSampleButton)
        clapFinishButton = findViewById(R.id.clapFinishButton)
        clapSaveButton = findViewById(R.id.clapSaveButton)

        musicModeText = findViewById(R.id.musicModeText)
        musicModeButtons = findViewById(R.id.musicModeButtons)
        musicBrightnessText = findViewById(R.id.musicBrightnessText)
        musicBrightnessSeek = findViewById(R.id.musicBrightnessSeek)
        musicBackgroundText = findViewById(R.id.musicBackgroundText)
        musicBackgroundSeek = findViewById(R.id.musicBackgroundSeek)
        musicSmoothingGroup = findViewById(R.id.musicSmoothingGroup)
        musicSmoothingText = findViewById(R.id.musicSmoothingText)
        musicSmoothingSeek = findViewById(R.id.musicSmoothingSeek)
        musicSensitivityGroup = findViewById(R.id.musicSensitivityGroup)
        musicSensitivityText = findViewById(R.id.musicSensitivityText)
        musicSensitivitySeek = findViewById(R.id.musicSensitivitySeek)
        musicSubmodeGroup = findViewById(R.id.musicSubmodeGroup)
        musicSpeedGroup = findViewById(R.id.musicSpeedGroup)
        musicSpeedText = findViewById(R.id.musicSpeedText)
        musicSpeedSeek = findViewById(R.id.musicSpeedSeek)
        musicAuxGroup = findViewById(R.id.musicAuxGroup)
        musicAuxText = findViewById(R.id.musicAuxText)
        musicAuxSeek = findViewById(R.id.musicAuxSeek)
        musicHueStartGroup = findViewById(R.id.musicHueStartGroup)
        musicHueStartText = findViewById(R.id.musicHueStartText)
        musicHueStartSeek = findViewById(R.id.musicHueStartSeek)
        musicCalibrationText = findViewById(R.id.musicCalibrationText)

        ambientEffectText = findViewById(R.id.ambientEffectText)
        ambientColorPreview = findViewById(R.id.ambientColorPreview)
        ambientHueText = findViewById(R.id.ambientHueText)
        ambientHueSeek = findViewById(R.id.ambientHueSeek)
        ambientSaturationGroup = findViewById(R.id.ambientSaturationGroup)
        ambientSaturationText = findViewById(R.id.ambientSaturationText)
        ambientSaturationSeek = findViewById(R.id.ambientSaturationSeek)
        ambientBrightnessText = findViewById(R.id.ambientBrightnessText)
        ambientBrightnessSeek = findViewById(R.id.ambientBrightnessSeek)
        ambientSpeedGroup = findViewById(R.id.ambientSpeedGroup)
        ambientSpeedText = findViewById(R.id.ambientSpeedText)
        ambientSpeedSeek = findViewById(R.id.ambientSpeedSeek)
        ambientRainbowGroup = findViewById(R.id.ambientRainbowGroup)
        ambientRainbowText = findViewById(R.id.ambientRainbowText)
        ambientRainbowSeek = findViewById(R.id.ambientRainbowSeek)
        ambientAutoSwitch = findViewById(R.id.ambientAutoSwitch)
        ambientPeriodText = findViewById(R.id.ambientPeriodText)
        ambientPeriodSeek = findViewById(R.id.ambientPeriodSeek)

        nightEnabledSwitch = findViewById(R.id.nightEnabledSwitch)
        nightColorPreview = findViewById(R.id.nightColorPreview)
        nightHueText = findViewById(R.id.nightHueText)
        nightHueSeek = findViewById(R.id.nightHueSeek)
        nightSaturationText = findViewById(R.id.nightSaturationText)
        nightSaturationSeek = findViewById(R.id.nightSaturationSeek)
        nightBrightnessText = findViewById(R.id.nightBrightnessText)
        nightBrightnessSeek = findViewById(R.id.nightBrightnessSeek)
        nightScheduleSwitch = findViewById(R.id.nightScheduleSwitch)
        nightOnInput = findViewById(R.id.nightOnInput)
        nightOffInput = findViewById(R.id.nightOffInput)

        alarmStateText = findViewById(R.id.alarmStateText)
        alarmStartPreview = findViewById(R.id.alarmStartPreview)
        alarmEndPreview = findViewById(R.id.alarmEndPreview)
        alarmEnabledSwitch = findViewById(R.id.alarmEnabledSwitch)
        alarmHourInput = findViewById(R.id.alarmHourInput)
        alarmMinuteInput = findViewById(R.id.alarmMinuteInput)
        alarmFadeText = findViewById(R.id.alarmFadeText)
        alarmFadeSeek = findViewById(R.id.alarmFadeSeek)
        alarmBrightnessText = findViewById(R.id.alarmBrightnessText)
        alarmBrightnessSeek = findViewById(R.id.alarmBrightnessSeek)
        alarmStartHueText = findViewById(R.id.alarmStartHueText)
        alarmStartHueSeek = findViewById(R.id.alarmStartHueSeek)
        alarmEndHueText = findViewById(R.id.alarmEndHueText)
        alarmEndHueSeek = findViewById(R.id.alarmEndHueSeek)

        addressInput = findViewById(R.id.addressInput)
        systemSummaryText = findViewById(R.id.systemSummaryText)
        currentLimitText = findViewById(R.id.currentLimitText)
        currentLimitSeek = findViewById(R.id.currentLimitSeek)
        eventsText = findViewById(R.id.eventsText)
        developerPanel = findViewById(R.id.developerPanel)
        developerToggleButton = findViewById(R.id.developerToggleButton)
        rawCommandInput = findViewById(R.id.rawCommandInput)
        rawResponseText = findViewById(R.id.rawResponseText)

        refreshButton.setOnClickListener { refreshDevice() }
    }

    private fun bindNavigation() {
        sectionNavigation = listOf(
            findViewById<Button>(R.id.navLightButton) to lightPanel,
            findViewById<Button>(R.id.navMusicButton) to musicPanel,
            findViewById<Button>(R.id.navAmbientButton) to ambientPanel,
            findViewById<Button>(R.id.navNightButton) to nightPanel,
            findViewById<Button>(R.id.navAlarmButton) to alarmPanel
        )

        sectionNavigation.forEach { (button, panel) ->
            button.setOnClickListener { showSection(panel) }
        }

        findViewById<Button>(R.id.navServiceButton).setOnClickListener {
            showSection(servicePanel)
        }
    }

    private fun showSection(target: LinearLayout) {
        listOf(lightPanel, musicPanel, ambientPanel, nightPanel, alarmPanel, servicePanel)
            .forEach { it.visibility = if (it === target) View.VISIBLE else View.GONE }

        sectionNavigation.forEach { (button, panel) ->
            button.isSelected = panel === target
            button.setTextColor(
                getColor(if (button.isSelected) R.color.ardu_accent else R.color.ardu_text_secondary)
            )
        }
    }

    private fun bindLight() {
        findViewById<Button>(R.id.lightOnButton).setOnClickListener {
            runDeviceAction("Включение света") { api.setLightEnabled(true) }
        }
        findViewById<Button>(R.id.lightOffButton).setOnClickListener {
            runDeviceAction("Выключение света") { api.setLightEnabled(false) }
        }
        findViewById<Button>(R.id.lightSceneEveningButton).setOnClickListener {
            applyLightScene("Вечер", 2200, 76)
        }
        findViewById<Button>(R.id.light2700Button).setOnClickListener {
            applyLightScene("Тёплый свет", 2700, 178)
        }
        findViewById<Button>(R.id.light4000Button).setOnClickListener {
            applyLightScene("Дневной свет", 4000, 204)
        }
        findViewById<Button>(R.id.light6000Button).setOnClickListener {
            applyLightScene("Холодный свет", 6000, 204)
        }

        findViewById<Button>(R.id.lightBrightnessMinusButton).setOnClickListener {
            changeLightBrightness(-26)
        }
        findViewById<Button>(R.id.lightBrightnessPlusButton).setOnClickListener {
            changeLightBrightness(26)
        }

        lightBrightnessSeek.configure(
            min = 0,
            max = 255,
            mode = SmartSliderView.VisualMode.BRIGHTNESS
        )
        lightBrightnessSeek.setListener(
            preview = { value ->
                lightBrightnessText.text = "${brightnessPercent(value)}%"
            },
            commit = { value ->
                runDeviceAction("Яркость света") { api.setLightBrightness(value) }
            }
        )

        lightKelvinSeek.configure(
            min = 1800,
            max = 6500,
            mode = SmartSliderView.VisualMode.KELVIN
        )
        lightKelvinSeek.setListener(
            preview = { value ->
                lightKelvinText.text = "$value K"
            },
            commit = { value ->
                setLightKelvin(value)
            }
        )

        val rgbLabel: (Int) -> Unit = { updateRgbLabel() }
        lightRedSeek.setOnSeekBarChangeListener(labelOnlyListener(rgbLabel))
        lightGreenSeek.setOnSeekBarChangeListener(labelOnlyListener(rgbLabel))
        lightBlueSeek.setOnSeekBarChangeListener(labelOnlyListener(rgbLabel))

        lightColorWheel.setListener(
            onPreview = { color ->
                rendering = true
                lightRedSeek.progress = Color.red(color)
                lightGreenSeek.progress = Color.green(color)
                lightBlueSeek.progress = Color.blue(color)
                rendering = false
                updateRgbLabel()
            },
            onCommit = { color ->
                runDeviceAction("Цвет света") {
                    api.setLightRgb(Color.red(color), Color.green(color), Color.blue(color))
                }
            }
        )

        findViewById<Button>(R.id.lightAdvancedRgbButton).setOnClickListener {
            val show = lightAdvancedRgbPanel.visibility != View.VISIBLE
            lightAdvancedRgbPanel.visibility = if (show) View.VISIBLE else View.GONE
            findViewById<Button>(R.id.lightAdvancedRgbButton).text =
                if (show) "Скрыть точную RGB настройку" else "Точная RGB настройка"
        }

        findViewById<Button>(R.id.lightApplyRgbButton).setOnClickListener {
            val red = lightRedSeek.progress
            val green = lightGreenSeek.progress
            val blue = lightBlueSeek.progress
            runDeviceAction("RGB свет") { api.setLightRgb(red, green, blue) }
        }

        findViewById<Button>(R.id.lightSaveProfileButton).setOnClickListener {
            runDeviceAction("Сохранение startup-профиля") { api.saveLightStartupProfile() }
        }

        clapEnabledSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) runDeviceAction("Хлопковый выключатель") { api.setClapEnabled(checked) }
        }

        findViewById<Button>(R.id.clapStartButton).setOnClickListener { startClapCalibration() }
        clapSampleButton.setOnClickListener { captureClapSample() }
        clapFinishButton.setOnClickListener { finishClapCalibration() }
        clapSaveButton.setOnClickListener {
            runDeviceAction("Сохранение порога хлопков") { api.saveClapCalibration() }
            clapCalibrationPanel.visibility = View.GONE
        }
        findViewById<Button>(R.id.clapCancelButton).setOnClickListener {
            runDeviceAction("Отмена калибровки хлопков") { api.cancelClapCalibration() }
            clapCalibrationPanel.visibility = View.GONE
        }
    }

    private fun setLightKelvin(value: Int) {
        runDeviceAction("Температура света") { api.setLightKelvin(value) }
    }

    private fun applyLightScene(label: String, kelvin: Int, brightness: Int) {
        runDeviceAction(label) {
            api.setLightEnabled(true)
            api.setLightKelvin(kelvin)
            api.setLightBrightness(brightness)
        }
    }

    private fun changeLightBrightness(delta: Int) {
        val value = (lightBrightnessSeek.value() + delta).coerceIn(0, 255)
        lightBrightnessSeek.setValue(value)
        lightBrightnessText.text = "${brightnessPercent(value)}%"
        runDeviceAction("Яркость света") { api.setLightBrightness(value) }
    }

    private fun brightnessPercent(value: Int): Int =
        ((value.coerceIn(0, 255) * 100) + 127) / 255

    private fun updateRgbLabel() {
        lightRgbText.text =
            "RGB: ${lightRedSeek.progress},${lightGreenSeek.progress},${lightBlueSeek.progress}"
        lightColorWheel.setColor(
            Color.rgb(lightRedSeek.progress, lightGreenSeek.progress, lightBlueSeek.progress)
        )
        setPreview(
            lightColorPreview,
            Color.rgb(lightRedSeek.progress, lightGreenSeek.progress, lightBlueSeek.progress)
        )
    }

    private fun updateAmbientPreview() {
        val effect = latestSettings?.ambient?.effect ?: "F01"
        val saturation = if (effect == "F03") 255 else ambientSaturationSeek.progress
        setPreview(
            ambientColorPreview,
            hueColor(ambientHueSeek.progress, saturation, ambientBrightnessSeek.progress)
        )
    }

    private fun updateNightPreview() {
        setPreview(
            nightColorPreview,
            hueColor(nightHueSeek.progress, nightSaturationSeek.progress, nightBrightnessSeek.progress)
        )
    }

    private fun updateAlarmPreviews() {
        setPreview(alarmStartPreview, hueColor(alarmStartHueSeek.progress, 255, 255))
        setPreview(alarmEndPreview, hueColor(alarmEndHueSeek.progress, 255, 255))
    }

    private fun hueColor(hue: Int, saturation: Int, brightness: Int): Int =
        Color.HSVToColor(
            floatArrayOf(
                hue.coerceIn(0, 255) * 360f / 255f,
                saturation.coerceIn(0, 255) / 255f,
                brightness.coerceIn(0, 255).coerceAtLeast(24) / 255f
            )
        )

    private fun setPreview(view: View, color: Int) {
        view.background.mutate().setTint(color)
    }

    private fun startClapCalibration() {
        clapCalibrationPanel.visibility = View.VISIBLE
        clapCalibrationText.text = "Соблюдайте тишину. Nano измеряет фон…"
        clapSampleButton.isEnabled = false
        clapFinishButton.isEnabled = false
        clapSaveButton.isEnabled = false

        worker.execute {
            try {
                val state = api.startClapCalibration(9)
                runOnUiThread {
                    clapCalibrationText.text =
                        "Тишина измерена: ${state.quietP99}. Прогресс 0/${state.targetPairs}. " +
                        "Нажмите кнопку и сделайте двойной хлопок."
                    clapSampleButton.isEnabled = true
                    operationText.text = "Калибровка хлопков запущена"
                }
            } catch (error: Exception) {
                showError("Калибровка хлопков", error)
            }
        }
    }

    private fun captureClapSample() {
        clapSampleButton.isEnabled = false
        clapCalibrationText.text = "Сделайте двойной хлопок…"
        worker.execute {
            try {
                val sample = api.captureClapSample()
                runOnUiThread {
                    if (sample.accepted) {
                        val good = sample.goodPairs ?: 0
                        val total = sample.targetPairs ?: 9
                        clapCalibrationText.text =
                            "Принято $good/$total. Сила: ${sample.weakStrength} / ${sample.strongStrength}"
                        clapFinishButton.isEnabled = good >= total
                    } else {
                        clapCalibrationText.text =
                            "Пара не принята. Обнаружено хлопков: ${sample.detectedClaps ?: 0}. Повторите."
                    }
                    clapSampleButton.isEnabled = !(sample.accepted &&
                        (sample.goodPairs ?: 0) >= (sample.targetPairs ?: 9))
                }
            } catch (error: Exception) {
                showError("Sample хлопков", error)
                runOnUiThread { clapSampleButton.isEnabled = true }
            }
        }
    }

    private fun finishClapCalibration() {
        clapFinishButton.isEnabled = false
        worker.execute {
            try {
                val result = api.finishClapCalibration()
                runOnUiThread {
                    clapCalibrationText.text =
                        "Предложенный порог: ${result.suggestedThreshold}. " +
                        "Тишина=${result.quietP99}, P20=${result.clapP20}. " +
                        "Проверьте двойной хлопок и выберите Сохранить или Отмена."
                    clapSaveButton.isEnabled = true
                    operationText.text = "Порог применён в RAM, ещё не сохранён"
                }
            } catch (error: Exception) {
                showError("Расчёт порога", error)
                runOnUiThread { clapFinishButton.isEnabled = true }
            }
        }
    }

    private fun bindMusic() {
        findViewById<Button>(R.id.musicOnButton).setOnClickListener {
            runDeviceAction("Включение светомузыки") { api.setMode("music") }
        }
        findViewById<Button>(R.id.musicOffButton).setOnClickListener {
            runDeviceAction("Выключение светомузыки") { api.setMode("off") }
        }

        val names = mapOf(
            "M01" to "M01 VU",
            "M02" to "M02 Радуга",
            "M03" to "M03 5 полос",
            "M04" to "M04 3 полосы",
            "M05" to "M05 Частота",
            "M08" to "M08 Бегущие",
            "M09" to "M09 Спектр"
        )
        ArduApiClient.MUSIC_IDS.forEach { id ->
            val button = Button(this).apply {
                text = names[id] ?: id
                setOnClickListener {
                    runDeviceAction("Музыкальный режим $id") { api.selectMusicMode(id) }
                }
            }
            musicModeButtons.addView(button)
        }

        bindSeek(musicBrightnessSeek,
            { musicBrightnessText.text = "Яркость эффекта: $it" },
            { value -> runDeviceAction("Яркость музыки") { api.updateMusicSettings(activeBrightness = value) } }
        )
        bindSeek(musicBackgroundSeek,
            { musicBackgroundText.text = "Фоновая яркость: $it" },
            { value -> runDeviceAction("Фон музыки") { api.updateMusicSettings(backgroundBrightness = value) } }
        )
        bindSeek(musicSmoothingSeek,
            { musicSmoothingText.text = "Плавность: $it" },
            { value -> runDeviceAction("Плавность музыки") { api.updateMusicSettings(smoothing = value) } }
        )
        bindSeek(musicSensitivitySeek,
            { musicSensitivityText.text = "Чувствительность: $it" },
            { value -> runDeviceAction("Чувствительность") { api.updateMusicSettings(sensitivity = value) } }
        )
        bindSeek(musicSpeedSeek,
            { musicSpeedText.text = "Скорость: $it" },
            { value -> runDeviceAction("Скорость M08") { api.updateMusicSettings(speed = value) } }
        )
        bindSeek(musicAuxSeek,
            { updateMusicAuxLabel(it) },
            { value -> applyMusicAux(value) }
        )
        bindSeek(musicHueStartSeek,
            { musicHueStartText.text = "Начальный цвет: $it" },
            { value -> runDeviceAction("Цвет M09") { api.updateMusicSettings(hueStart = value) } }
        )

        findViewById<Button>(R.id.musicSubThree).setOnClickListener { setMusicSubmode("three") }
        findViewById<Button>(R.id.musicSubLow).setOnClickListener { setMusicSubmode("low") }
        findViewById<Button>(R.id.musicSubMid).setOnClickListener { setMusicSubmode("mid") }
        findViewById<Button>(R.id.musicSubHigh).setOnClickListener { setMusicSubmode("high") }

        findViewById<Button>(R.id.musicCalibrateButton).setOnClickListener {
            musicCalibrationText.text = "Калибровка… сохраняйте тишину."
            worker.execute {
                try {
                    val result = api.calibrateAudio()
                    runOnUiThread {
                        musicCalibrationText.text =
                            "Готово: DC=${result.micDc}, VU=${result.vuLowPass}, Spectrum=${result.spectrumLowPass}"
                        refreshDevice()
                    }
                } catch (error: Exception) {
                    showError("Калибровка микрофона", error)
                }
            }
        }
    }

    private fun setMusicSubmode(value: String) {
        runDeviceAction("Подрежим музыки") { api.updateMusicSettings(submode = value) }
    }

    private fun applyMusicAux(value: Int) {
        when (latestSettings?.music?.selected) {
            "M02" -> runDeviceAction("Шаг радуги M02") {
                api.updateMusicSettings(rainbowStep10 = value)
            }
            "M09" -> runDeviceAction("Шаг цвета M09") {
                api.updateMusicSettings(hueStep = value)
            }
        }
    }

    private fun updateMusicAuxLabel(value: Int) {
        musicAuxText.text = when (latestSettings?.music?.selected) {
            "M02" -> "Скорость радуги: $value"
            "M09" -> "Шаг цвета: $value"
            else -> "Параметр: $value"
        }
    }

    private fun bindAmbient() {
        findViewById<Button>(R.id.ambientOnButton).setOnClickListener {
            runDeviceAction("Включение фона") { api.setMode("ambient") }
        }
        findViewById<Button>(R.id.ambientOffButton).setOnClickListener {
            runDeviceAction("Выключение фона") { api.setMode("off") }
        }
        findViewById<Button>(R.id.ambientF01Button).setOnClickListener { setAmbientEffect("F01") }
        findViewById<Button>(R.id.ambientF02Button).setOnClickListener { setAmbientEffect("F02") }
        findViewById<Button>(R.id.ambientF03Button).setOnClickListener { setAmbientEffect("F03") }

        bindSeek(ambientHueSeek,
            {
                ambientHueText.text = "Цвет: $it"
                updateAmbientPreview()
            },
            { value -> runDeviceAction("Цвет фона") { api.updateAmbientSettings(hue = value) } }
        )
        bindSeek(ambientSaturationSeek,
            {
                ambientSaturationText.text = "Насыщенность: $it"
                updateAmbientPreview()
            },
            { value -> runDeviceAction("Насыщенность фона") { api.updateAmbientSettings(saturation = value) } }
        )
        bindSeek(ambientBrightnessSeek,
            {
                ambientBrightnessText.text = "Яркость: $it"
                updateAmbientPreview()
            },
            { value -> runDeviceAction("Яркость фона") { api.updateAmbientSettings(brightness = value) } }
        )
        bindSeek(ambientSpeedSeek,
            { ambientSpeedText.text = "Скорость: $it" },
            { value -> runDeviceAction("Скорость фона") { api.updateAmbientSettings(speed = value) } }
        )
        bindSeek(ambientRainbowSeek,
            { ambientRainbowText.text = String.format(Locale.US, "Шаг радуги: %.1f", it / 10.0) },
            { value -> runDeviceAction("Шаг радуги") { api.updateAmbientSettings(rainbowStep = value / 10.0) } }
        )
        bindSeek(ambientPeriodSeek,
            { ambientPeriodText.text = "Период: $it с" },
            { value -> runDeviceAction("Период автоперебора") { api.updateAmbientSettings(autoPeriodSec = value) } }
        )

        ambientAutoSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) {
                runDeviceAction("Автоперебор фона") { api.updateAmbientSettings(autoCycle = checked) }
            }
        }
    }

    private fun setAmbientEffect(id: String) {
        runDeviceAction("Фоновый эффект $id") { api.selectAmbientEffect(id) }
    }

    private fun bindNight() {
        nightEnabledSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) runDeviceAction("Ночник") { api.updateNightSettings(enabled = checked) }
        }
        nightScheduleSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) runDeviceAction("Расписание ночника") {
                api.updateNightSettings(scheduleEnabled = checked)
            }
        }

        bindSeek(nightHueSeek,
            {
                nightHueText.text = "Цвет: $it"
                updateNightPreview()
            },
            { value -> runDeviceAction("Цвет ночника") { api.updateNightSettings(hue = value) } }
        )
        bindSeek(nightSaturationSeek,
            {
                nightSaturationText.text = "Насыщенность: $it"
                updateNightPreview()
            },
            { value -> runDeviceAction("Насыщенность ночника") { api.updateNightSettings(saturation = value) } }
        )
        bindSeek(nightBrightnessSeek,
            {
                nightBrightnessText.text = "Яркость: $it"
                updateNightPreview()
            },
            { value -> runDeviceAction("Яркость ночника") { api.updateNightSettings(brightness = value) } }
        )

        findViewById<Button>(R.id.nightSaveScheduleButton).setOnClickListener {
            val on = nightOnInput.text.toString().trim()
            val off = nightOffInput.text.toString().trim()
            runDeviceAction("Время ночника") {
                api.updateNightSettings(scheduleOn = on, scheduleOff = off)
            }
        }
    }

    private fun bindAlarm() {
        alarmEnabledSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) runDeviceAction("Будильник") { api.updateAlarmSettings(enabled = checked) }
        }

        findViewById<Button>(R.id.alarmSaveTimeButton).setOnClickListener {
            val hour = alarmHourInput.text.toString().toIntOrNull()
            val minute = alarmMinuteInput.text.toString().toIntOrNull()
            if (hour == null || minute == null || hour !in 0..23 || minute !in 0..59) {
                operationText.text = "Неверное время будильника"
                return@setOnClickListener
            }
            runDeviceAction("Время будильника") {
                api.updateAlarmSettings(hour = hour, minute = minute)
            }
        }

        bindSeek(alarmFadeSeek,
            { alarmFadeText.text = "Рассвет: $it мин" },
            { value -> runDeviceAction("Длительность рассвета") { api.updateAlarmSettings(fadeMinutes = value) } }
        )
        bindSeek(alarmBrightnessSeek,
            { alarmBrightnessText.text = "Макс. яркость: $it" },
            { value -> runDeviceAction("Яркость рассвета") { api.updateAlarmSettings(maxBrightness = value) } }
        )
        bindSeek(alarmStartHueSeek,
            {
                alarmStartHueText.text = "Начальный цвет: $it"
                updateAlarmPreviews()
            },
            { value -> runDeviceAction("Начальный цвет рассвета") { api.updateAlarmSettings(startHue = value) } }
        )
        bindSeek(alarmEndHueSeek,
            {
                alarmEndHueText.text = "Конечный цвет: $it"
                updateAlarmPreviews()
            },
            { value -> runDeviceAction("Конечный цвет рассвета") { api.updateAlarmSettings(endHue = value) } }
        )

        findViewById<Button>(R.id.syncTimeButton).setOnClickListener {
            runDeviceAction("Синхронизация RTC") { api.syncTime() }
        }
        findViewById<Button>(R.id.stopDawnButton).setOnClickListener {
            runDeviceAction("Остановка рассвета") { api.stopDawn() }
        }
    }

    private fun bindService() {
        findViewById<Button>(R.id.saveAddressButton).setOnClickListener {
            val raw = addressInput.text.toString().trim()
            val normalized = if (raw.isBlank()) null else {
                if (raw.startsWith("http://") || raw.startsWith("https://")) raw else "http://$raw"
            }
            preferences().edit().putString(PREF_ADDRESS, normalized).apply()
            api.setPreferredAddress(normalized)
            refreshDevice()
        }

        bindSeek(currentLimitSeek,
            { currentLimitText.text = "Лимит тока: $it мА" },
            { value -> runDeviceAction("Лимит тока") { api.setCurrentLimit(value) } }
        )

        findViewById<Button>(R.id.eventsButton).setOnClickListener {
            worker.execute {
                try {
                    val events = api.events()
                    runOnUiThread {
                        eventsText.text = if (events.isEmpty()) {
                            "Событий нет"
                        } else {
                            events.joinToString("\n") {
                                "${it.type} code=${it.code} args=${it.args} • ${it.raw}"
                            }
                        }
                    }
                } catch (error: Exception) {
                    showError("События", error)
                }
            }
        }

        developerToggleButton.setOnClickListener {
            val show = developerPanel.visibility != View.VISIBLE
            developerPanel.visibility = if (show) View.VISIBLE else View.GONE
            developerToggleButton.text = if (show) "Скрыть Developer Mode" else "Developer Mode"
        }

        findViewById<Button>(R.id.sendRawButton).setOnClickListener {
            val command = rawCommandInput.text.toString()
            worker.execute {
                try {
                    val response = api.sendNanoCommand(command)
                    runOnUiThread {
                        rawResponseText.text =
                            "command=${response.command}\n${response.nano}\nerror=${response.errorCode}"
                    }
                } catch (error: Exception) {
                    showError("Developer UART", error)
                }
            }
        }
    }

    private fun refreshDevice() {
        connectionText.text = "Проверка связи…"
        refreshButton.isEnabled = false
        operationText.text = ""

        worker.execute {
            try {
                val ping = api.ping()
                lastEspFirmware = ping.firmware
                lastEspRssi = ping.rssi
                val snapshot = readSnapshot()
                val selectedAddress = api.selectedAddress()
                if (!selectedAddress.isNullOrBlank()) {
                    preferences().edit().putString(PREF_ADDRESS, selectedAddress).apply()
                }

                runOnUiThread {
                    connectionText.text = "● Онлайн"
                    connectionText.setTextColor(getColor(R.color.ardu_accent))
                    if (!selectedAddress.isNullOrBlank()) {
                        addressInput.setText(selectedAddress.removePrefix("http://"))
                    }
                    renderSnapshot(snapshot)
                    refreshButton.isEnabled = true
                }
            } catch (error: Exception) {
                runOnUiThread {
                    connectionText.text = "● Нет связи"
                    connectionText.setTextColor(getColor(R.color.ardu_danger))
                    operationText.text = error.message ?: "Ошибка подключения"
                    refreshButton.isEnabled = true
                }
            }
        }
    }

    private fun readSnapshot(): Snapshot =
        Snapshot(api.status(), api.settings(), api.time())

    private fun runDeviceAction(label: String, action: () -> Unit) {
        operationText.text = "$label…"
        worker.execute {
            try {
                action()
                val snapshot = readSnapshot()
                runOnUiThread {
                    renderSnapshot(snapshot)
                    operationText.text = "$label: готово"
                }
            } catch (error: Exception) {
                showError(label, error)
            }
        }
    }

    private fun renderSnapshot(snapshot: Snapshot) {
        rendering = true
        latestSettings = snapshot.settings

        modeText.text = modeTitle(snapshot.status.mode)
        timeText.text = if (snapshot.time.valid) {
            snapshot.time.time?.take(5) ?: "--:--"
        } else {
            "RTC —"
        }

        renderLight(snapshot)
        renderMusic(snapshot.settings)
        renderAmbient(snapshot.settings)
        renderNight(snapshot.settings)
        renderAlarm(snapshot.settings)
        renderService(snapshot)

        rendering = false
    }

    private fun renderLight(snapshot: Snapshot) {
        val l = snapshot.settings.light
        lightStateText.text =
            "${if (snapshot.status.mode == "light") "Включён" else "Выключен"} • " +
            "${if (l.colorMode == "kelvin") "${l.kelvin} K" else "RGB"} • " +
            "${brightnessPercent(l.brightness)}%" +
            if (l.dirty) " • изменения не сохранены" else ""

        lightBrightnessSeek.setValue(l.brightness)
        lightKelvinSeek.setValue(l.kelvin)
        lightRedSeek.progress = l.red
        lightGreenSeek.progress = l.green
        lightBlueSeek.progress = l.blue
        lightBrightnessText.text = "${brightnessPercent(l.brightness)}%"
        lightKelvinText.text = "${l.kelvin} K"
        updateRgbLabel()

        val clap = snapshot.settings.clap
        clapEnabledSwitch.isChecked = clap.enabled
        clapStateText.text =
            "Порог ${clap.threshold}, окно ${clap.timeoutMs} мс • " +
            if (clap.saved) "сохранён" else "не сохранён"

        if (clap.calibration.active || clap.calibration.finished) {
            clapCalibrationPanel.visibility = View.VISIBLE
            clapCalibrationText.text =
                "Калибровка ${clap.calibration.goodPairs}/${clap.calibration.targetPairs}, " +
                "тишина=${clap.calibration.quietP99}, порог=${clap.calibration.suggestedThreshold}"
        }
    }

    private fun renderMusic(settings: ArduSettings) {
        val id = settings.music.selected
        val cfg = settings.music.modes[id] ?: return
        musicModeText.text = "Режим: $id"
        musicBrightnessSeek.progress = cfg.brightness
        musicBackgroundSeek.progress = cfg.backgroundBrightness
        musicSmoothingSeek.progress = cfg.smoothing.coerceIn(5, 100)
        musicSensitivitySeek.progress = cfg.sensitivity.coerceIn(50, 200)
        musicSpeedSeek.progress = cfg.speed.coerceIn(1, 255)

        musicBrightnessText.text = "Яркость эффекта: ${cfg.brightness}"
        musicBackgroundText.text = "Фоновая яркость: ${cfg.backgroundBrightness}"
        musicSmoothingText.text = "Плавность: ${cfg.smoothing}"
        musicSensitivityText.text = "Чувствительность: ${cfg.sensitivity}"
        musicSpeedText.text = "Скорость: ${cfg.speed}"

        updateMusicVisibility(id)

        when (id) {
            "M02" -> {
                musicAuxSeek.min = 5
                musicAuxSeek.max = 200
                musicAuxSeek.progress = cfg.aux.coerceIn(5, 200)
            }
            "M09" -> {
                musicAuxSeek.min = 1
                musicAuxSeek.max = 255
                musicAuxSeek.progress = cfg.aux.coerceIn(1, 255)
                musicHueStartSeek.progress = cfg.speed.coerceIn(0, 255)
                musicHueStartText.text = "Начальный цвет: ${cfg.speed}"
            }
        }
        updateMusicAuxLabel(musicAuxSeek.progress)

        val sys = settings.system
        musicCalibrationText.text =
            if (sys.audioCalibrated) {
                "Калибровано: DC=${sys.micDc}, VU=${sys.vuLowPass}, Spectrum=${sys.spectrumLowPass}"
            } else {
                "Микрофон ещё не откалиброван"
            }
    }

    private fun updateMusicVisibility(id: String) {
        musicSmoothingGroup.visibility =
            if (id in setOf("M01", "M02", "M03", "M04", "M05")) View.VISIBLE else View.GONE
        musicSensitivityGroup.visibility =
            if (id in setOf("M03", "M04", "M05", "M08")) View.VISIBLE else View.GONE
        musicSubmodeGroup.visibility =
            if (id == "M05" || id == "M08") View.VISIBLE else View.GONE
        musicSpeedGroup.visibility = if (id == "M08") View.VISIBLE else View.GONE
        musicAuxGroup.visibility = if (id == "M02" || id == "M09") View.VISIBLE else View.GONE
        musicHueStartGroup.visibility = if (id == "M09") View.VISIBLE else View.GONE
    }

    private fun renderAmbient(settings: ArduSettings) {
        val a = settings.ambient
        val cfg = a.effects[a.effect] ?: return
        ambientEffectText.text = "Эффект: ${a.effect}"
        ambientHueSeek.progress = cfg.hue
        ambientBrightnessSeek.progress = cfg.brightness
        ambientHueText.text = "Цвет: ${cfg.hue}"
        ambientBrightnessText.text = "Яркость: ${cfg.brightness}"

        cfg.saturation?.let {
            ambientSaturationSeek.progress = it
            ambientSaturationText.text = "Насыщенность: $it"
        }
        cfg.speed?.let {
            ambientSpeedSeek.progress = it.coerceIn(1, 255)
            ambientSpeedText.text = "Скорость: $it"
        }
        cfg.rainbowStep?.let {
            ambientRainbowSeek.progress = (it * 10.0).toInt().coerceIn(5, 100)
            ambientRainbowText.text = String.format(Locale.US, "Шаг радуги: %.1f", it)
        }
        ambientAutoSwitch.isChecked = a.autoCycle
        ambientPeriodSeek.progress = a.autoPeriodSec
        ambientPeriodText.text = "Период: ${a.autoPeriodSec} с"

        ambientSaturationGroup.visibility = if (a.effect == "F03") View.GONE else View.VISIBLE
        ambientSpeedGroup.visibility = if (a.effect == "F01") View.GONE else View.VISIBLE
        ambientRainbowGroup.visibility = if (a.effect == "F03") View.VISIBLE else View.GONE
        updateAmbientPreview()
    }

    private fun renderNight(settings: ArduSettings) {
        val n = settings.night
        nightEnabledSwitch.isChecked = n.enabled
        nightHueSeek.progress = n.hue
        nightSaturationSeek.progress = n.saturation
        nightBrightnessSeek.progress = n.brightness
        nightScheduleSwitch.isChecked = n.scheduleEnabled
        nightOnInput.setText(n.scheduleOn)
        nightOffInput.setText(n.scheduleOff)
        nightHueText.text = "Цвет: ${n.hue}"
        nightSaturationText.text = "Насыщенность: ${n.saturation}"
        nightBrightnessText.text = "Яркость: ${n.brightness}"
        updateNightPreview()
    }

    private fun renderAlarm(settings: ArduSettings) {
        val a = settings.alarm
        alarmEnabledSwitch.isChecked = a.enabled
        alarmHourInput.setText(a.hour.toString())
        alarmMinuteInput.setText(a.minute.toString())
        alarmFadeSeek.progress = a.fadeMinutes
        alarmBrightnessSeek.progress = a.maxBrightness
        alarmStartHueSeek.progress = a.startHue
        alarmEndHueSeek.progress = a.endHue
        alarmFadeText.text = "Рассвет: ${a.fadeMinutes} мин"
        alarmBrightnessText.text = "Макс. яркость: ${a.maxBrightness}"
        alarmStartHueText.text = "Начальный цвет: ${a.startHue}"
        alarmEndHueText.text = "Конечный цвет: ${a.endHue}"
        alarmStateText.text =
            "Будильник ${if (a.enabled) "включён" else "выключен"} • " +
            String.format(Locale.US, "%02d:%02d", a.hour, a.minute) +
            " • рассвет ${a.dawnPhase}" +
            if (a.recovered) " • восстановлен после reset" else ""
        updateAlarmPreviews()
    }

    private fun renderService(snapshot: Snapshot) {
        val status = snapshot.status
        val sys = snapshot.settings.system
        currentLimitSeek.progress = sys.currentLimitMa
        currentLimitText.text = "Лимит тока: ${sys.currentLimitMa} мА"
        systemSummaryText.text = buildString {
            appendLine("ESP: $lastEspFirmware")
            lastEspRssi?.let { appendLine("Wi-Fi: $it dBm") }
            appendLine("Nano: ${status.nanoFirmware}")
            appendLine("UART: v${status.uartProtocol}")
            appendLine("RTC: ${if (status.rtcValid) "OK" else "INVALID"}")
            appendLine("Mode: ${status.mode} (${status.modeId})")
            appendLine("Clap: ${status.clapThreshold} / ${status.clapTimeoutMs} ms")
            appendLine("Music: ${status.musicMode}")
            appendLine("Ambient: ${status.ambientEffect}")
            appendLine("Current limit: ${status.currentLimitMa} mA")
            appendLine("Audio: ${if (sys.audioCalibrated) "CALIBRATED" else "NOT CALIBRATED"}")
            append("Uptime: ${status.uptimeMs} ms")
        }
    }

    private fun modeTitle(mode: String): String = when (mode) {
        "off" -> "Выключено"
        "light" -> "Обычный свет"
        "night" -> "Ночник"
        "dawn" -> "Рассвет"
        "ambient" -> "Фон"
        "music" -> "Светомузыка"
        else -> mode
    }

    private fun bindSeek(
        seek: SeekBar,
        label: (Int) -> Unit,
        commit: (Int) -> Unit
    ) {
        seek.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                label(progress)
            }
            override fun onStartTrackingTouch(seekBar: SeekBar) = Unit
            override fun onStopTrackingTouch(seekBar: SeekBar) {
                if (!rendering) commit(seekBar.progress)
            }
        })
    }

    private fun labelOnlyListener(label: (Int) -> Unit) =
        object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar, progress: Int, fromUser: Boolean) {
                label(progress)
            }
            override fun onStartTrackingTouch(seekBar: SeekBar) = Unit
            override fun onStopTrackingTouch(seekBar: SeekBar) = Unit
        }

    private fun showError(label: String, error: Exception) {
        runOnUiThread {
            operationText.text = "$label: ${error.message ?: "ошибка"}"
            refreshButton.isEnabled = true
        }
    }

    private fun preferences() =
        getSharedPreferences("ardu_connection", MODE_PRIVATE)

    private data class Snapshot(
        val status: DeviceStatus,
        val settings: ArduSettings,
        val time: TimeStatus
    )

    companion object {
        private const val PREF_ADDRESS = "preferred_address"
    }
}
