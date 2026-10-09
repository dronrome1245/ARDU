package com.ardu.app

import android.app.Activity
import android.app.AlertDialog
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.graphics.Color
import android.provider.Settings
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.SeekBar
import android.widget.ScrollView
import android.widget.Switch
import android.widget.TextView
import com.ardu.app.net.ArduApiClient
import com.ardu.app.net.ArduSettings
import com.ardu.app.net.DeviceStatus
import com.ardu.app.net.TimeStatus
import com.ardu.app.ui.AmbientEffectTileView
import com.ardu.app.ui.ColorWheelView
import com.ardu.app.ui.FocusDialView
import com.ardu.app.ui.FocalCropImageView
import com.ardu.app.ui.MusicModeTileView
import com.ardu.app.ui.RoomHeroView
import com.ardu.app.ui.SceneTileView
import com.ardu.app.ui.SmartSliderView
import java.util.Locale
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private val api = ArduApiClient()
    private val worker = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())
    private var heartbeatEnabled = false
    private var heartbeatInFlight = false
    private var refreshInProgress = false
    private var lastConnectionHealth = DeviceConnectionHealth.DISCONNECTED
    private val heartbeatRunnable = object : Runnable {
        override fun run() {
            if (!heartbeatEnabled) return
            checkDeviceHeartbeat()
            mainHandler.postDelayed(this, DeviceConnectionPolicy.HEARTBEAT_PERIOD_MS)
        }
    }
    private var rendering = false
    private var showServiceMessages = false
    private var lastOperationImportant = false
    private var clapWizardDismissed = false
    private var deviceOnline = false
    private lateinit var contentScrollView: ScrollView
    private var latestSettings: ArduSettings? = null
    private var latestMode: String? = null
    private var sectionNavigation: List<Pair<ImageButton, LinearLayout>> = emptyList()
    private var sectionLabels: List<Pair<TextView, LinearLayout>> = emptyList()
    private var lastUserPanel: LinearLayout? = null
    private var lastEspFirmware: String = "—"
    private var lastEspRssi: Int? = null
    private var lastNetworkMode: String = "unknown"
    private val musicModeButtonMap = linkedMapOf<String, MusicModeTileView>()
    private var ambientEffectButtonMap: Map<String, AmbientEffectTileView> = emptyMap()
    private var musicSubmodeButtonMap: Map<Int, Button> = emptyMap()

    private lateinit var globalHeaderPanel: LinearLayout
    private lateinit var connectionText: TextView
    private lateinit var modeText: TextView
    private lateinit var timeText: TextView
    private lateinit var operationText: TextView
    private lateinit var refreshButton: Button

    private lateinit var lightPanel: LinearLayout
    private lateinit var lightOverviewPanel: LinearLayout
    private lateinit var lightControlPanel: LinearLayout
    private lateinit var lightConnectionText: TextView
    private lateinit var lightOperationText: TextView
    private lateinit var lightHeroImage: FocalCropImageView
    private lateinit var lightRefreshButton: Button
    private lateinit var lightServiceButton: Button
    private lateinit var lightFocusDial: FocusDialView
    private lateinit var lightOverviewBrightnessText: TextView
    private lateinit var lightOverviewKelvinText: TextView
    private lateinit var lightOverviewOnButton: Button
    private lateinit var lightOverviewOffButton: Button
    private lateinit var lightOnButton: Button
    private lateinit var lightOffButton: Button
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

    private lateinit var musicConnectionText: TextView
    private lateinit var musicOperationText: TextView
    private lateinit var musicHeroImage: ImageView
    private lateinit var musicRefreshButton: Button
    private lateinit var musicServiceButton: Button
    private lateinit var musicModeText: TextView
    private lateinit var musicModeButtons: LinearLayout
    private lateinit var musicStartButton: Button
    private lateinit var musicStopButton: Button
    private lateinit var musicBrightnessText: TextView
    private lateinit var musicBrightnessSeek: SmartSliderView
    private lateinit var musicBackgroundText: TextView
    private lateinit var musicBackgroundSeek: SmartSliderView
    private lateinit var musicSmoothingGroup: LinearLayout
    private lateinit var musicSmoothingText: TextView
    private lateinit var musicSmoothingSeek: SmartSliderView
    private lateinit var musicSensitivityGroup: LinearLayout
    private lateinit var musicSensitivityText: TextView
    private lateinit var musicSensitivitySeek: SmartSliderView
    private lateinit var musicSubmodeGroup: LinearLayout
    private lateinit var musicSpeedGroup: LinearLayout
    private lateinit var musicSpeedText: TextView
    private lateinit var musicSpeedSeek: SmartSliderView
    private lateinit var musicAuxGroup: LinearLayout
    private lateinit var musicAuxText: TextView
    private lateinit var musicAuxSeek: SmartSliderView
    private lateinit var musicHueStartGroup: LinearLayout
    private lateinit var musicHueStartText: TextView
    private lateinit var musicHueStartSeek: SmartSliderView
    private lateinit var musicCalibrationText: TextView

    private lateinit var ambientConnectionText: TextView
    private lateinit var ambientOperationText: TextView
    private lateinit var ambientHeroView: RoomHeroView
    private lateinit var ambientRefreshButton: Button
    private lateinit var ambientServiceButton: Button
    private lateinit var ambientEffectText: TextView
    private lateinit var ambientColorPreview: View
    private lateinit var ambientHueText: TextView
    private lateinit var ambientHueSeek: SmartSliderView
    private lateinit var ambientSaturationGroup: LinearLayout
    private lateinit var ambientSaturationText: TextView
    private lateinit var ambientSaturationSeek: SmartSliderView
    private lateinit var ambientBrightnessText: TextView
    private lateinit var ambientBrightnessSeek: SmartSliderView
    private lateinit var ambientSpeedGroup: LinearLayout
    private lateinit var ambientSpeedText: TextView
    private lateinit var ambientSpeedSeek: SmartSliderView
    private lateinit var ambientRainbowGroup: LinearLayout
    private lateinit var ambientRainbowText: TextView
    private lateinit var ambientRainbowSeek: SmartSliderView
    private lateinit var ambientAutoSwitch: Switch
    private lateinit var ambientPeriodText: TextView
    private lateinit var ambientPeriodSeek: SmartSliderView
    private lateinit var ambientOnButton: Button
    private lateinit var ambientOffButton: Button
    private lateinit var ambientPresetButtons: Map<String, ImageButton>
    private lateinit var ambientPresetLabels: Map<String, TextView>
    private lateinit var ambientPresetExtraGrid: LinearLayout
    private lateinit var ambientPresetCapabilityText: TextView
    private lateinit var ambientPresetControls: LinearLayout
    private lateinit var ambientPresetStatusText: TextView
    private lateinit var ambientPresetBrightnessText: TextView
    private lateinit var ambientPresetBrightnessSeek: SmartSliderView
    private lateinit var ambientPresetDynamicsText: TextView
    private lateinit var ambientPresetDynamicsSeek: SmartSliderView
    private lateinit var ambientManualPanel: LinearLayout
    private val ambientExtraPresetButtons = linkedMapOf<String, ImageButton>()
    private val ambientExtraPresetLabels = linkedMapOf<String, TextView>()
    private val ambientSceneImageResources = mapOf(
        "P01" to R.drawable.ardu_preset_p01,
        "P02" to R.drawable.ardu_preset_p02,
        "P03" to R.drawable.ardu_preset_p03,
        "P04" to R.drawable.ardu_preset_p04,
        "P05" to R.drawable.ardu_preset_p05,
        "P06" to R.drawable.ardu_preset_p06,
        "P07" to R.drawable.ardu_preset_p07,
        "P08" to R.drawable.ardu_preset_p08,
        "P09" to R.drawable.ardu_preset_p09,
        "P10" to R.drawable.ardu_preset_p10,
        "P11" to R.drawable.ardu_preset_p11,
        "P12" to R.drawable.ardu_preset_p12
    )
    private val ambientSceneNames = linkedMapOf(
        "P01" to "Северное сияние", "P02" to "Закат на Бали",
        "P03" to "Океан", "P04" to "Космос",
        "P05" to "Камин", "P06" to "Свечи",
        "P07" to "Лунный свет", "P08" to "Лес",
        "P09" to "Неон", "P10" to "Лава",
        "P11" to "Дыхание", "P12" to "Радуга"
    )

    private lateinit var nightConnectionText: TextView
    private lateinit var nightOperationText: TextView
    private lateinit var nightHeroView: RoomHeroView
    private lateinit var nightRefreshButton: Button
    private lateinit var nightServiceButton: Button
    private lateinit var nightStateText: TextView
    private lateinit var nightEnabledSwitch: Switch
    private lateinit var nightColorPreview: View
    private lateinit var nightHueText: TextView
    private lateinit var nightHueSeek: SmartSliderView
    private lateinit var nightSaturationText: TextView
    private lateinit var nightSaturationSeek: SmartSliderView
    private lateinit var nightBrightnessText: TextView
    private lateinit var nightBrightnessSeek: SmartSliderView
    private lateinit var nightScheduleSwitch: Switch
    private lateinit var nightOnInput: EditText
    private lateinit var nightOffInput: EditText

    private lateinit var alarmConnectionText: TextView
    private lateinit var alarmOperationText: TextView
    private lateinit var alarmHeroView: RoomHeroView
    private lateinit var alarmRefreshButton: Button
    private lateinit var alarmServiceButton: Button
    private lateinit var alarmStateText: TextView
    private lateinit var stopDawnButton: Button
    private lateinit var alarmRtcText: TextView
    private lateinit var alarmStartPreview: View
    private lateinit var alarmEndPreview: View
    private lateinit var alarmEnabledSwitch: Switch
    private lateinit var alarmHourInput: EditText
    private lateinit var alarmMinuteInput: EditText
    private lateinit var alarmFadeText: TextView
    private lateinit var alarmFadeSeek: SmartSliderView
    private lateinit var alarmBrightnessText: TextView
    private lateinit var alarmBrightnessSeek: SmartSliderView
    private lateinit var alarmStartHueText: TextView
    private lateinit var alarmStartHueSeek: SmartSliderView
    private lateinit var alarmEndHueText: TextView
    private lateinit var alarmEndHueSeek: SmartSliderView

    private lateinit var addressInput: EditText
    private lateinit var systemSummaryText: TextView
    private lateinit var currentLimitText: TextView
    private lateinit var currentLimitSeek: SeekBar
    private lateinit var eventsText: TextView
    private lateinit var developerPanel: LinearLayout
    private lateinit var developerToggleButton: Button
    private lateinit var showServiceMessagesSwitch: Switch
    private lateinit var rawCommandInput: EditText
    private lateinit var rawResponseText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        bindViews()
        clapWizardDismissed = preferences().getBoolean(PREF_CLAP_WIZARD_DISMISSED, false)
        bindNavigation()
        bindLight()
        bindMusic()
        bindAmbient()
        bindNight()
        bindAlarm()
        bindService()
        showUnavailableDeviceState()

        val savedAddress = preferences().getString(PREF_ADDRESS, null)
        if (!savedAddress.isNullOrBlank()) {
            api.setPreferredAddress(savedAddress)
            addressInput.setText(savedAddress.removePrefix("http://"))
        }

        showLightOverview()
        refreshDevice()
    }

    override fun onStart() {
        super.onStart()
        heartbeatEnabled = true
        mainHandler.removeCallbacks(heartbeatRunnable)
        mainHandler.post(heartbeatRunnable)
    }

    override fun onStop() {
        heartbeatEnabled = false
        mainHandler.removeCallbacks(heartbeatRunnable)
        super.onStop()
    }

    override fun onDestroy() {
        heartbeatEnabled = false
        mainHandler.removeCallbacks(heartbeatRunnable)
        worker.shutdownNow()
        super.onDestroy()
    }

    private fun bindViews() {
        contentScrollView = findViewById(R.id.contentScrollView)
        globalHeaderPanel = findViewById(R.id.globalHeaderPanel)
        connectionText = findViewById(R.id.connectionText)
        modeText = findViewById(R.id.modeText)
        timeText = findViewById(R.id.timeText)
        operationText = findViewById(R.id.operationText)
        refreshButton = findViewById(R.id.refreshButton)

        lightPanel = findViewById(R.id.lightPanel)
        lightOverviewPanel = findViewById(R.id.lightOverviewPanel)
        lightControlPanel = findViewById(R.id.lightControlPanel)
        lightConnectionText = findViewById(R.id.lightConnectionText)
        lightOperationText = findViewById(R.id.lightOperationText)
        lightHeroImage = findViewById(R.id.lightHeroImage)
        lightRefreshButton = findViewById(R.id.lightRefreshButton)
        lightServiceButton = findViewById(R.id.lightServiceButton)
        lightFocusDial = findViewById(R.id.lightFocusDial)
        lightOverviewBrightnessText = findViewById(R.id.lightOverviewBrightnessText)
        lightOverviewKelvinText = findViewById(R.id.lightOverviewKelvinText)
        lightOverviewOnButton = findViewById(R.id.lightOverviewOnButton)
        lightOverviewOffButton = findViewById(R.id.lightOverviewOffButton)
        lightOnButton = findViewById(R.id.lightOnButton)
        lightOffButton = findViewById(R.id.lightOffButton)
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

        musicConnectionText = findViewById(R.id.musicConnectionText)
        musicOperationText = findViewById(R.id.musicOperationText)
        musicHeroImage = findViewById(R.id.musicHeroImage)
        musicRefreshButton = findViewById(R.id.musicRefreshButton)
        musicServiceButton = findViewById(R.id.musicServiceButton)
        musicModeText = findViewById(R.id.musicModeText)
        musicModeButtons = findViewById(R.id.musicModeButtons)
        musicStartButton = findViewById(R.id.musicStartButton)
        musicStopButton = findViewById(R.id.musicStopButton)
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

        ambientConnectionText = findViewById(R.id.ambientConnectionText)
        ambientOperationText = findViewById(R.id.ambientOperationText)
        ambientHeroView = findViewById(R.id.ambientHeroView)
        ambientRefreshButton = findViewById(R.id.ambientRefreshButton)
        ambientServiceButton = findViewById(R.id.ambientServiceButton)
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
        ambientOnButton = findViewById(R.id.ambientOnButton)
        ambientOffButton = findViewById(R.id.ambientOffButton)

        nightConnectionText = findViewById(R.id.nightConnectionText)
        nightOperationText = findViewById(R.id.nightOperationText)
        nightHeroView = findViewById(R.id.nightHeroView)
        nightRefreshButton = findViewById(R.id.nightRefreshButton)
        nightServiceButton = findViewById(R.id.nightServiceButton)
        nightStateText = findViewById(R.id.nightStateText)
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

        alarmConnectionText = findViewById(R.id.alarmConnectionText)
        alarmOperationText = findViewById(R.id.alarmOperationText)
        alarmHeroView = findViewById(R.id.alarmHeroView)
        alarmRefreshButton = findViewById(R.id.alarmRefreshButton)
        alarmServiceButton = findViewById(R.id.alarmServiceButton)
        alarmStateText = findViewById(R.id.alarmStateText)
        stopDawnButton = findViewById(R.id.stopDawnButton)
        alarmRtcText = findViewById(R.id.alarmRtcText)
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
        showServiceMessagesSwitch = findViewById(R.id.showServiceMessagesSwitch)
        rawCommandInput = findViewById(R.id.rawCommandInput)
        rawResponseText = findViewById(R.id.rawResponseText)

        refreshButton.setOnClickListener { refreshDevice() }
    }

    private fun navigateToUserSection(target: LinearLayout) {
        lastUserPanel = target

        if (target === lightPanel) {
            showLightOverview()
        } else {
            showSection(target)
        }

        val targetMode = when (target) {
            lightPanel -> "light"
            musicPanel -> "music"
            ambientPanel -> "ambient"
            nightPanel -> "night"
            alarmPanel -> "off"
            else -> null
        }

        if (targetMode == null || !deviceOnline || latestMode == targetMode) return

        setOperationStatus(
            when (target) {
                lightPanel -> "Включение света…"
                musicPanel -> "Запуск светомузыки…"
                ambientPanel -> "Запуск фона…"
                nightPanel -> "Переход в ночной режим…"
                alarmPanel -> "Остановка текущего режима…"
                else -> "Переключение режима…"
            }
        )

        worker.execute {
            try {
                api.setMode(targetMode)
                val snapshot = readSnapshot()
                runOnUiThread {
                    renderSnapshot(snapshot)
                    setOperationStatus("")
                }
            } catch (error: Exception) {
                showError("Переключение режима", error)
            }
        }
    }

    private fun bindNavigation() {
        sectionNavigation = listOf(
            findViewById<ImageButton>(R.id.navLightButton) to lightPanel,
            findViewById<ImageButton>(R.id.navMusicButton) to musicPanel,
            findViewById<ImageButton>(R.id.navAmbientButton) to ambientPanel,
            findViewById<ImageButton>(R.id.navNightButton) to nightPanel,
            findViewById<ImageButton>(R.id.navAlarmButton) to alarmPanel
        )
        sectionLabels = listOf(
            findViewById<TextView>(R.id.navLightLabel) to lightPanel,
            findViewById<TextView>(R.id.navMusicLabel) to musicPanel,
            findViewById<TextView>(R.id.navAmbientLabel) to ambientPanel,
            findViewById<TextView>(R.id.navNightLabel) to nightPanel,
            findViewById<TextView>(R.id.navAlarmLabel) to alarmPanel
        )

        sectionNavigation.forEach { (button, panel) ->
            button.setOnClickListener {
                navigateToUserSection(panel)
            }
        }

        findViewById<Button>(R.id.navServiceButton).setOnClickListener {
            openService()
        }
        findViewById<Button>(R.id.serviceBackButton).setOnClickListener {
            closeService()
        }
    }

    private fun openService() {
        val visible = listOf(lightPanel, musicPanel, ambientPanel, nightPanel, alarmPanel)
            .firstOrNull { it.visibility == View.VISIBLE }
        if (visible != null) lastUserPanel = visible
        showSection(servicePanel)
    }

    private fun closeService() {
        val target = lastUserPanel ?: lightPanel
        if (target === lightPanel) showLightOverview() else showSection(target)
    }

    private fun showLightOverview() {
        showSection(lightPanel)
        lightOverviewPanel.visibility = View.VISIBLE
        lightControlPanel.visibility = View.GONE
    }

    private fun showLightControl() {
        showSection(lightPanel)
        lightOverviewPanel.visibility = View.GONE
        lightControlPanel.visibility = View.VISIBLE
    }

    private fun showSection(target: LinearLayout) {
        listOf(lightPanel, musicPanel, ambientPanel, nightPanel, alarmPanel, servicePanel)
            .forEach { it.visibility = if (it === target) View.VISIBLE else View.GONE }

        globalHeaderPanel.visibility =
            if (target in listOf(lightPanel, musicPanel, ambientPanel, nightPanel, alarmPanel)) View.GONE
            else View.VISIBLE

        sectionNavigation.forEach { (button, panel) ->
            button.isSelected = panel === target
        }
        sectionLabels.forEach { (label, panel) ->
            label.isSelected = panel === target
        }
        // Each tab opens at its top, not at the prior tab's scroll offset.
        contentScrollView.scrollTo(0, 0)
    }

    private fun bindLight() {
        // Keep the warm floor lamp inside the nearly-square Home Hub hero crop.
        lightHeroImage.setFocus(0.64f, 0.50f)

        lightRefreshButton.setOnClickListener { refreshDevice() }
        lightServiceButton.setOnClickListener { openService() }
        findViewById<Button>(R.id.lightOpenControlButton).setOnClickListener { showLightControl() }
        findViewById<Button>(R.id.lightBackButton).setOnClickListener { showLightOverview() }

        lightFocusDial.setListener(
            preview = { value ->
                lightBrightnessSeek.setValue(value)
                lightBrightnessText.text = "${brightnessPercent(value)}%"
                lightOverviewBrightnessText.text = "${brightnessPercent(value)}%"
            },
            commit = { value ->
                runDeviceAction("Яркость света") { api.setLightBrightness(value) }
            }
        )

        lightOnButton.setOnClickListener {
            runDeviceAction("Включение света") { api.setLightEnabled(true) }
        }
        lightOffButton.setOnClickListener {
            runDeviceAction("Выключение света") { api.setLightEnabled(false) }
        }
        lightOverviewOnButton.setOnClickListener {
            runDeviceAction("Включение света") { api.setLightEnabled(true) }
        }
        lightOverviewOffButton.setOnClickListener {
            runDeviceAction("Выключение света") { api.setLightEnabled(false) }
        }
        findViewById<SceneTileView>(R.id.lightSceneEveningButton).apply {
            setScene(SceneTileView.Scene.EVENING)
            setOnClickListener { applyLightScene("Вечер", 2200, 38) }
        }
        findViewById<SceneTileView>(R.id.light2700Button).apply {
            setScene(SceneTileView.Scene.WARM)
            setOnClickListener { applyLightScene("Кино", 2700, 90) }
        }
        findViewById<SceneTileView>(R.id.light4000Button).apply {
            setScene(SceneTileView.Scene.DAY)
            setOnClickListener { applyLightScene("Гости", 3500, 178) }
        }
        findViewById<SceneTileView>(R.id.light6000Button).apply {
            setScene(SceneTileView.Scene.COOL)
            setOnClickListener { applyLightScene("Чтение", 4300, 230) }
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
            runDeviceAction(
                "Сохранение порога хлопков",
                onAcknowledged = { setClapWizardDismissed(true) }
            ) { api.saveClapCalibration() }
        }
        findViewById<Button>(R.id.clapCancelButton).setOnClickListener {
            runDeviceAction("Отмена калибровки хлопков") { api.cancelClapCalibration() }
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
        val value = (lightFocusDial.value() + delta).coerceIn(0, 255)
        lightFocusDial.setValue(value)
        lightBrightnessSeek.setValue(value)
        lightBrightnessText.text = "${brightnessPercent(value)}%"
        lightOverviewBrightnessText.text = "${brightnessPercent(value)}%"
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
        val saturation = if (effect == "F03") 255 else ambientSaturationSeek.value()
        setPreview(
            ambientColorPreview,
            hueColor(ambientHueSeek.value(), saturation, ambientBrightnessSeek.value())
        )
    }

    private fun updateNightPreview() {
        setPreview(
            nightColorPreview,
            hueColor(nightHueSeek.value(), nightSaturationSeek.value(), nightBrightnessSeek.value())
        )
    }

    private fun updateAlarmPreviews() {
        setPreview(alarmStartPreview, hueColor(alarmStartHueSeek.value(), 255, 255))
        setPreview(alarmEndPreview, hueColor(alarmEndHueSeek.value(), 255, 255))
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

    private fun setClapWizardDismissed(dismissed: Boolean) {
        clapWizardDismissed = dismissed
        preferences().edit().putBoolean(PREF_CLAP_WIZARD_DISMISSED, dismissed).apply()
        if (dismissed) clapCalibrationPanel.visibility = View.GONE
    }

    private fun startClapCalibration() {
        if (!deviceOnline) return

        clapCalibrationPanel.visibility = View.VISIBLE
        clapCalibrationText.text = "Соблюдайте тишину. Nano измеряет фон…"
        clapSampleButton.isEnabled = false
        clapFinishButton.isEnabled = false
        clapSaveButton.isEnabled = false

        worker.execute {
            try {
                val state = api.startClapCalibration(9)
                runOnUiThread {
                    setClapWizardDismissed(false)
                    clapCalibrationPanel.visibility = View.VISIBLE
                    clapCalibrationText.text =
                        "Тишина измерена: ${state.quietP99}. Прогресс 0/${state.targetPairs}. " +
                        "Нажмите кнопку и сделайте двойной хлопок."
                    clapSampleButton.isEnabled = true
                    setOperationStatus("Калибровка хлопков запущена")
                }
            } catch (error: Exception) {
                showError("Калибровка хлопков", error)
            }
        }
    }

    private fun captureClapSample() {
        if (!deviceOnline) return

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
        if (!deviceOnline) return

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
                    setOperationStatus("Порог применён в RAM, ещё не сохранён")
                }
            } catch (error: Exception) {
                showError("Расчёт порога", error)
                runOnUiThread { clapFinishButton.isEnabled = true }
            }
        }
    }

    private fun bindMusic() {
        musicRefreshButton.setOnClickListener { refreshDevice() }
        musicServiceButton.setOnClickListener { openService() }
        musicStartButton.setOnClickListener {
            runDeviceAction("Запуск светомузыки") { api.setMode("music") }
        }
        musicStopButton.setOnClickListener {
            runDeviceAction("Остановка светомузыки") { api.setMode("off") }
        }

        val names = mapOf(
            "M01" to "Градиент",
            "M02" to "Радуга",
            "M03" to "5 полос",
            "M04" to "3 полосы",
            "M05" to "Частота",
            "M08" to "Бегущие",
            "M09" to "Спектр"
        )

        musicModeButtonMap.clear()
        musicModeButtons.removeAllViews()

        val rows = listOf(
            ArduApiClient.MUSIC_IDS.take(4),
            ArduApiClient.MUSIC_IDS.drop(4)
        )

        rows.forEachIndexed { rowIndex, ids ->
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    if (rowIndex > 0) topMargin = dp(8)
                }
            }

            ids.forEachIndexed { index, id ->
                val tile = MusicModeTileView(this).apply {
                    configure(id, names[id] ?: id)
                    layoutParams = LinearLayout.LayoutParams(0, dp(104), 1f).apply {
                        if (index > 0) marginStart = dp(4)
                        if (index < ids.lastIndex) marginEnd = dp(4)
                    }
                    setOnClickListener {
                        runDeviceAction("Музыкальный режим $id") { api.selectMusicMode(id) }
                    }
                }
                musicModeButtonMap[id] = tile
                row.addView(tile)
            }

            musicModeButtons.addView(row)
        }

        bindSmartSlider(
            musicBrightnessSeek, 0, 255, SmartSliderView.VisualMode.BRIGHTNESS,
            { value -> musicBrightnessText.text = "Эффект • ${brightnessPercent(value)}%" },
            { value -> runDeviceAction("Яркость музыки") { api.updateMusicSettings(activeBrightness = value) } }
        )
        bindSmartSlider(
            musicBackgroundSeek, 0, 255, SmartSliderView.VisualMode.BRIGHTNESS,
            { value -> musicBackgroundText.text = "Фон • ${brightnessPercent(value)}%" },
            { value -> runDeviceAction("Фон музыки") { api.updateMusicSettings(backgroundBrightness = value) } }
        )
        bindSmartSlider(
            musicSmoothingSeek, 5, 100, SmartSliderView.VisualMode.ACCENT,
            { value -> musicSmoothingText.text = "Плавность • $value" },
            { value -> runDeviceAction("Плавность музыки") { api.updateMusicSettings(smoothing = value) } }
        )
        bindSmartSlider(
            musicSensitivitySeek, 50, 200, SmartSliderView.VisualMode.ACCENT,
            { value -> musicSensitivityText.text = "Чувствительность • $value" },
            { value -> runDeviceAction("Чувствительность") { api.updateMusicSettings(sensitivity = value) } }
        )
        bindSmartSlider(
            musicSpeedSeek, 1, 255, SmartSliderView.VisualMode.ACCENT,
            { value -> musicSpeedText.text = "Скорость • $value" },
            { value -> runDeviceAction("Скорость M08") { api.updateMusicSettings(speed = value) } }
        )
        bindSmartSlider(
            musicAuxSeek, 1, 255, SmartSliderView.VisualMode.ACCENT,
            { value -> updateMusicAuxLabel(value) },
            { value -> applyMusicAux(value) }
        )
        bindSmartSlider(
            musicHueStartSeek, 0, 255, SmartSliderView.VisualMode.HUE,
            { value -> musicHueStartText.text = "Цвет • $value" },
            { value -> runDeviceAction("Цвет M09") { api.updateMusicSettings(hueStart = value) } }
        )

        musicSubmodeButtonMap = mapOf(
            0 to findViewById<Button>(R.id.musicSubThree),
            1 to findViewById<Button>(R.id.musicSubLow),
            2 to findViewById<Button>(R.id.musicSubMid),
            3 to findViewById<Button>(R.id.musicSubHigh)
        )
        musicSubmodeButtonMap[0]?.setOnClickListener { setMusicSubmode("three") }
        musicSubmodeButtonMap[1]?.setOnClickListener { setMusicSubmode("low") }
        musicSubmodeButtonMap[2]?.setOnClickListener { setMusicSubmode("mid") }
        musicSubmodeButtonMap[3]?.setOnClickListener { setMusicSubmode("high") }

        findViewById<Button>(R.id.musicCalibrateButton).setOnClickListener {
            musicCalibrationText.text = "Калибровка… тишина"
            worker.execute {
                try {
                    val result = api.calibrateAudio()
                    runOnUiThread {
                        musicCalibrationText.text =
                            "DC ${result.micDc} • VU ${result.vuLowPass} • Spectrum ${result.spectrumLowPass}"
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
            "M02" -> "Радуга • $value"
            "M09" -> "Шаг цвета • $value"
            else -> "Параметр • $value"
        }
    }

    private fun bindAmbient() {
        ambientHeroView.setScene(RoomHeroView.Scene.AMBIENT)
        ambientRefreshButton.setOnClickListener { refreshDevice() }
        ambientServiceButton.setOnClickListener { openService() }

        ambientOnButton.setOnClickListener {
            runDeviceAction("Включение фона") { api.setMode("ambient") }
        }
        ambientOffButton.setOnClickListener {
            runDeviceAction("Выключение фона") { api.setMode("off") }
        }

        ambientPresetButtons = mapOf(
            "P01" to findViewById(R.id.ambientPresetAurora),
            "P02" to findViewById(R.id.ambientPresetSunset),
            "P03" to findViewById(R.id.ambientPresetOcean),
            "P04" to findViewById(R.id.ambientPresetCosmos)
        )
        ambientPresetLabels = mapOf(
            "P01" to findViewById(R.id.ambientPresetAuroraLabel),
            "P02" to findViewById(R.id.ambientPresetSunsetLabel),
            "P03" to findViewById(R.id.ambientPresetOceanLabel),
            "P04" to findViewById(R.id.ambientPresetCosmosLabel)
        )
        ambientPresetButtons.forEach { (id, button) ->
            button.setOnClickListener { applyAmbientPreset(id) }
        }
        ambientPresetLabels.forEach { (id, label) ->
            label.setOnClickListener { applyAmbientPreset(id) }
        }
        ambientPresetExtraGrid = findViewById(R.id.ambientPresetExtraGrid)
        ambientPresetCapabilityText = findViewById(R.id.ambientPresetCapabilityText)
        ambientPresetControls = findViewById(R.id.ambientPresetControls)
        ambientPresetStatusText = findViewById(R.id.ambientPresetStatusText)
        ambientPresetBrightnessText = findViewById(R.id.ambientPresetBrightnessText)
        ambientPresetBrightnessSeek = findViewById(R.id.ambientPresetBrightnessSeek)
        ambientPresetDynamicsText = findViewById(R.id.ambientPresetDynamicsText)
        ambientPresetDynamicsSeek = findViewById(R.id.ambientPresetDynamicsSeek)
        ambientManualPanel = ambientHueSeek.parent as LinearLayout
        findViewById<View>(R.id.ambientF02Button).visibility = View.GONE
        findViewById<View>(R.id.ambientF03Button).visibility = View.GONE
        (ambientAutoSwitch.parent as LinearLayout).visibility = View.GONE
        ambientEffectButtonMap = mapOf(
            "F01" to findViewById<AmbientEffectTileView>(R.id.ambientF01Button)
        )
        ambientEffectButtonMap.forEach { (id, tile) ->
            tile.configure(id, "Цвет")
            tile.setOnClickListener { setAmbientEffect(id) }
        }
        // All 12 tiles now use the same four-column photo grid. The top row
        // is declared in XML; P05..P12 are added using identical ImageButtons.
        ambientSceneNames.keys.drop(4).chunked(4).forEach { group ->
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { topMargin = dp(9) }
            }
            group.forEach { id ->
                val tile = LinearLayout(this).apply {
                    orientation = LinearLayout.VERTICAL
                    gravity = android.view.Gravity.CENTER_HORIZONTAL
                }
                val image = ImageButton(this).apply {
                    setBackgroundResource(R.drawable.ardu_preset_thumb)
                    clipToOutline = true
                    scaleType = ImageView.ScaleType.CENTER_CROP
                    setPadding(0, 0, 0, 0)
                    setImageResource(ambientSceneImageResources.getValue(id))
                    contentDescription = ambientSceneNames.getValue(id)
                    setOnClickListener { applyAmbientPreset(id) }
                }
                tile.addView(image, LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, dp(76)
                ))
                val label = TextView(this).apply {
                    text = ambientSceneNames.getValue(id)
                    textSize = 11f
                    setTextColor(getColor(R.color.ardu_nav_icon_tint))
                    gravity = android.view.Gravity.CENTER
                    maxLines = 2
                    minHeight = dp(32)
                    setOnClickListener { applyAmbientPreset(id) }
                }
                tile.addView(label, LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { topMargin = dp(5) })
                row.addView(tile, LinearLayout.LayoutParams(
                    0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f
                ).apply {
                    marginStart = dp(3)
                    marginEnd = dp(3)
                })
                ambientExtraPresetButtons[id] = image
                ambientExtraPresetLabels[id] = label
            }
            ambientPresetExtraGrid.addView(row)
        }
        bindSmartSlider(
            ambientPresetBrightnessSeek, 0, 255, SmartSliderView.VisualMode.BRIGHTNESS,
            { ambientPresetBrightnessText.text = "Яркость • ${brightnessPercent(it)}%" },
            { value -> runDeviceAction("Яркость сцены") {
                api.updateAmbientPresetSettings(brightness = value)
            } }
        )
        bindSmartSlider(
            ambientPresetDynamicsSeek, 0, 255, SmartSliderView.VisualMode.ACCENT,
            { ambientPresetDynamicsText.text = "Динамика • $it" },
            { value -> runDeviceAction("Динамика сцены") {
                api.updateAmbientPresetSettings(dynamics = value)
            } }
        )

        bindSmartSlider(
            ambientHueSeek, 0, 255, SmartSliderView.VisualMode.HUE,
            {
                ambientHueText.text = "Цвет • $it"
                updateAmbientPreview()
            },
            { value -> runDeviceAction("Цвет фона") { api.updateAmbientSettings(hue = value) } }
        )
        bindSmartSlider(
            ambientSaturationSeek, 0, 255, SmartSliderView.VisualMode.ACCENT,
            {
                ambientSaturationText.text = "Насыщенность • ${brightnessPercent(it)}%"
                updateAmbientPreview()
            },
            { value -> runDeviceAction("Насыщенность фона") { api.updateAmbientSettings(saturation = value) } }
        )
        bindSmartSlider(
            ambientBrightnessSeek, 0, 255, SmartSliderView.VisualMode.BRIGHTNESS,
            {
                ambientBrightnessText.text = "Яркость • ${brightnessPercent(it)}%"
                updateAmbientPreview()
            },
            { value -> runDeviceAction("Яркость фона") { api.updateAmbientSettings(brightness = value) } }
        )
        bindSmartSlider(
            ambientSpeedSeek, 1, 255, SmartSliderView.VisualMode.ACCENT,
            { ambientSpeedText.text = "Скорость • $it" },
            { value -> runDeviceAction("Скорость фона") { api.updateAmbientSettings(speed = value) } }
        )
        bindSmartSlider(
            ambientRainbowSeek, 5, 100, SmartSliderView.VisualMode.ACCENT,
            { ambientRainbowText.text = String.format(Locale.US, "Шаг • %.1f", it / 10.0) },
            { value -> runDeviceAction("Шаг радуги") { api.updateAmbientSettings(rainbowStep = value / 10.0) } }
        )
        bindSmartSlider(
            ambientPeriodSeek, 1, 255, SmartSliderView.VisualMode.ACCENT,
            { ambientPeriodText.text = "Период • $it с" },
            { value -> runDeviceAction("Период автоперебора") { api.updateAmbientSettings(autoPeriodSec = value) } }
        )

        ambientAutoSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) {
                runDeviceAction("Автоперебор фона") { api.updateAmbientSettings(autoCycle = checked) }
            }
        }
    }

    private fun setAmbientEffect(id: String) {
        setAmbientPresetSelection(null)
        runDeviceAction("Фоновый эффект $id") { api.selectAmbientEffect(id) }
    }

    private fun applyAmbientPreset(id: String) {
        val label = ambientSceneNames[id] ?: return
        runDeviceAction(label) { api.selectAmbientPreset(id) }
    }

    private fun setAmbientPresetSelection(selectedId: String?) {
        ambientPresetButtons.forEach { (id, button) ->
            button.isSelected = id == selectedId
        }
        ambientPresetLabels.forEach { (id, label) ->
            label.isSelected = id == selectedId
            label.setTextColor(getColor(
                if (id == selectedId) R.color.ardu_accent else R.color.ardu_nav_icon_tint
            ))
        }
        ambientExtraPresetButtons.forEach { (id, button) ->
            button.isSelected = id == selectedId
            button.invalidate()
        }
        ambientExtraPresetLabels.forEach { (id, label) ->
            label.isSelected = id == selectedId
            label.setTextColor(getColor(
                if (id == selectedId) R.color.ardu_accent else R.color.ardu_nav_icon_tint
            ))
        }
    }

    private fun bindNight() {
        nightHeroView.setScene(RoomHeroView.Scene.NIGHT)
        nightRefreshButton.setOnClickListener { refreshDevice() }
        nightServiceButton.setOnClickListener { openService() }

        nightEnabledSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) runDeviceAction("Ночник") { api.updateNightSettings(enabled = checked) }
        }
        nightScheduleSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) runDeviceAction("Расписание ночника") {
                api.updateNightSettings(scheduleEnabled = checked)
            }
        }

        bindSmartSlider(
            nightHueSeek, 0, 255, SmartSliderView.VisualMode.HUE,
            {
                nightHueText.text = "Цвет • $it"
                updateNightPreview()
            },
            { value -> runDeviceAction("Цвет ночника") { api.updateNightSettings(hue = value) } }
        )
        bindSmartSlider(
            nightSaturationSeek, 0, 255, SmartSliderView.VisualMode.ACCENT,
            {
                nightSaturationText.text = "Насыщенность • ${brightnessPercent(it)}%"
                updateNightPreview()
            },
            { value -> runDeviceAction("Насыщенность ночника") { api.updateNightSettings(saturation = value) } }
        )
        bindSmartSlider(
            nightBrightnessSeek, 0, 255, SmartSliderView.VisualMode.BRIGHTNESS,
            {
                nightBrightnessText.text = "Яркость • ${brightnessPercent(it)}%"
                updateNightPreview()
            },
            { value -> runDeviceAction("Яркость ночника") { api.updateNightSettings(brightness = value) } }
        )

        findViewById<Button>(R.id.nightSaveScheduleButton).setOnClickListener {
            val on = nightOnInput.text.toString().trim()
            val off = nightOffInput.text.toString().trim()
            val validTime = Regex("^(?:[01][0-9]|2[0-3]):[0-5][0-9]$")
            if (!validTime.matches(on) || !validTime.matches(off)) {
                setOperationStatus("Расписание: укажите время ЧЧ:ММ (00:00–23:59)", important = true)
                return@setOnClickListener
            }
            runDeviceAction("Время ночника") {
                api.updateNightSettings(scheduleOn = on, scheduleOff = off)
            }
        }
    }

    private fun bindAlarm() {
        alarmHeroView.setScene(RoomHeroView.Scene.DAWN)
        alarmRefreshButton.setOnClickListener { refreshDevice() }
        alarmServiceButton.setOnClickListener { openService() }

        alarmEnabledSwitch.setOnCheckedChangeListener { _, checked ->
            if (!rendering) runDeviceAction("Будильник") { api.updateAlarmSettings(enabled = checked) }
        }

        findViewById<Button>(R.id.alarmSaveTimeButton).setOnClickListener {
            val hour = alarmHourInput.text.toString().toIntOrNull()
            val minute = alarmMinuteInput.text.toString().toIntOrNull()
            if (hour == null || minute == null || hour !in 0..23 || minute !in 0..59) {
                setOperationStatus("Неверное время будильника", important = true)
                return@setOnClickListener
            }
            runDeviceAction("Время будильника") {
                api.updateAlarmSettings(hour = hour, minute = minute)
            }
        }

        bindSmartSlider(
            alarmFadeSeek, 1, 120, SmartSliderView.VisualMode.ACCENT,
            { alarmFadeText.text = "$it мин" },
            { value -> runDeviceAction("Длительность рассвета") { api.updateAlarmSettings(fadeMinutes = value) } }
        )
        bindSmartSlider(
            alarmBrightnessSeek, 1, 255, SmartSliderView.VisualMode.BRIGHTNESS,
            { alarmBrightnessText.text = "${brightnessPercent(it)}%" },
            { value -> runDeviceAction("Яркость рассвета") { api.updateAlarmSettings(maxBrightness = value) } }
        )
        bindSmartSlider(
            alarmStartHueSeek, 0, 255, SmartSliderView.VisualMode.HUE,
            {
                alarmStartHueText.text = "Начало • $it"
                updateAlarmPreviews()
            },
            { value -> runDeviceAction("Начальный цвет рассвета") { api.updateAlarmSettings(startHue = value) } }
        )
        bindSmartSlider(
            alarmEndHueSeek, 0, 255, SmartSliderView.VisualMode.HUE,
            {
                alarmEndHueText.text = "Финиш • $it"
                updateAlarmPreviews()
            },
            { value -> runDeviceAction("Конечный цвет рассвета") { api.updateAlarmSettings(endHue = value) } }
        )

        findViewById<Button>(R.id.syncTimeButton).setOnClickListener {
            runDeviceAction("Синхронизация RTC") { api.syncTime() }
        }
        stopDawnButton.setOnClickListener {
            runDeviceAction("Остановка рассвета") { api.stopDawn() }
        }
    }

    private fun bindService() {
        showServiceMessages = preferences().getBoolean(PREF_SHOW_SERVICE_MESSAGES, false)
        showServiceMessagesSwitch.isChecked = showServiceMessages
        showServiceMessagesSwitch.setOnCheckedChangeListener { _, show ->
            showServiceMessages = show
            preferences().edit().putBoolean(PREF_SHOW_SERVICE_MESSAGES, show).apply()
            updateServiceMessageVisibility()
        }
        updateServiceMessageVisibility()

        findViewById<Button>(R.id.openWifiSettingsButton).setOnClickListener {
            val intent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                Intent(Settings.Panel.ACTION_WIFI)
            } else {
                Intent(Settings.ACTION_WIFI_SETTINGS)
            }
            startActivity(intent)
        }

        findViewById<Button>(R.id.resetDefaultsButton).setOnClickListener {
            confirmResetDefaults()
        }

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
            { value ->
                if (UiSafetyPolicy.needsCurrentLimitConfirmation(value)) {
                    AlertDialog.Builder(this)
                        .setTitle("Повышенный лимит тока")
                        .setMessage(
                            "Два кольца 44+44 ещё не прошли силовой тест. " +
                                "До измерений рекомендуется не превышать 3000 мА. " +
                                "Установить $value мА только после проверки питания и проводки?"
                        )
                        .setNegativeButton("Отмена") { _, _ ->
                            currentLimitSeek.progress =
                                latestSettings?.system?.currentLimitMa ?: UiSafetyPolicy.UNTESTED_LIMIT_MA
                        }
                        .setPositiveButton("Применить") { _, _ ->
                            runDeviceAction("Лимит тока") { api.setCurrentLimit(value) }
                        }
                        .show()
                } else {
                    runDeviceAction("Лимит тока") { api.setCurrentLimit(value) }
                }
            }
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

    private fun confirmResetDefaults() {
        AlertDialog.Builder(this)
            .setTitle("Сбросить настройки?")
            .setMessage(
                "Будут восстановлены значения света, хлопка, музыки, фона, ночника, " +
                    "будильника и лимита тока. Время RTC останется без изменений."
            )
            .setNegativeButton("Отмена", null)
            .setPositiveButton("Сбросить") { _, _ ->
                runDeviceAction("Сброс настроек") { api.resetDefaults() }
            }
            .show()
    }

    private fun setOperationStatus(text: String, important: Boolean = false) {
        lastOperationImportant = important
        listOf(
            operationText, lightOperationText, musicOperationText,
            ambientOperationText, nightOperationText, alarmOperationText
        ).forEach { it.text = text }
        updateServiceMessageVisibility()
    }

    private fun updateServiceMessageVisibility() {
        // Connection badges are intentionally not controlled by this preference.
        val showOperation = (showServiceMessages || lastOperationImportant) &&
            operationText.text.isNotBlank()
        listOf(
            operationText, lightOperationText, musicOperationText,
            ambientOperationText, nightOperationText, alarmOperationText
        ).forEach { it.visibility = if (showOperation) View.VISIBLE else View.GONE }

        clapStateText.visibility = if (showServiceMessages) View.VISIBLE else View.GONE
        // Missing calibration / unsupported preset firmware are actionable warnings.
        musicCalibrationText.visibility =
            if (showServiceMessages ||
                (deviceOnline && latestSettings?.system?.audioCalibrated == false)
            ) View.VISIBLE else View.GONE
        ambientPresetCapabilityText.visibility =
            if (showServiceMessages ||
                (deviceOnline && latestSettings?.ambientPresets?.supported == false)
            ) View.VISIBLE else View.GONE
    }

    private fun setConnectionStatus(text: String, colorRes: Int) {
        connectionText.text = text
        lightConnectionText.text = text
        musicConnectionText.text = text
        ambientConnectionText.text = text
        nightConnectionText.text = text
        alarmConnectionText.text = text
        val color = getColor(colorRes)
        connectionText.setTextColor(color)
        lightConnectionText.setTextColor(color)
        musicConnectionText.setTextColor(color)
        ambientConnectionText.setTextColor(color)
        nightConnectionText.setTextColor(color)
        alarmConnectionText.setTextColor(color)
    }


    private fun showUnavailableDeviceState() {
        deviceOnline = false
        latestMode = null
        latestSettings = null
        setDeviceControlsEnabled(false)
        lightFocusDial.setKnown(false)
        lightOverviewBrightnessText.text = "—"
        lightOverviewKelvinText.text = "—"
        lightStateText.text = "Нет данных • подключите ARDU"
        lightBrightnessText.text = "Яркость • —"
        lightKelvinText.text = "— K"
        lightRgbText.text = "RGB: —"
        clapStateText.text = "Калибровка: нет данных"
        clapCalibrationPanel.visibility = View.GONE
        musicModeText.text = "Нет данных о режиме"
        musicBrightnessText.text = "Эффект • —"
        musicBackgroundText.text = "Фон • —"
        musicCalibrationText.text = "Микрофон: нет данных"
        ambientEffectText.text = "Нет данных о режиме"
        ambientPresetCapabilityText.text = "12 сцен • подключите ARDU для управления"
        ambientManualPanel.visibility = View.GONE
        ambientPresetControls.visibility = View.GONE
        nightStateText.text = "Нет данных о ночнике"
        nightEnabledSwitch.text = "Ночник: нет данных"
        nightHueText.text = "Цвет • —"
        nightSaturationText.text = "Насыщенность • —"
        nightBrightnessText.text = "Яркость • —"
        nightOnInput.setText("")
        nightOffInput.setText("")
        alarmStateText.text = "Нет данных о будильнике"
        alarmRtcText.text = "Время устройства неизвестно"
        alarmFadeText.text = "Длительность • —"
        alarmBrightnessText.text = "Макс. яркость • —"
        alarmHourInput.setText("")
        alarmMinuteInput.setText("")
        timeText.text = "RTC —"
        modeText.text = "Состояние неизвестно"
        currentLimitText.text = "Лимит тока: нет данных"
        systemSummaryText.text = "ARDU не подключена"
        stopDawnButton.isEnabled = false
        updateServiceMessageVisibility()
    }

    private fun setDeviceControlsEnabled(online: Boolean) {
        // Always allow navigation, refresh and connection repair while offline.
        val uiOnly = setOf(
            R.id.lightRefreshButton, R.id.lightServiceButton,
            R.id.musicRefreshButton, R.id.musicServiceButton,
            R.id.ambientRefreshButton, R.id.ambientServiceButton,
            R.id.nightRefreshButton, R.id.nightServiceButton,
            R.id.alarmRefreshButton, R.id.alarmServiceButton,
            R.id.lightOpenControlButton, R.id.lightBackButton
        )
        fun visit(view: View) {
            if (view.id in uiOnly) return
            if (view is ViewGroup) {
                for (index in 0 until view.childCount) visit(view.getChildAt(index))
            }
            if (
                view is Button || view is ImageButton || view is Switch ||
                view is SeekBar || view is EditText || view is SmartSliderView ||
                view is FocusDialView || view is ColorWheelView ||
                view is SceneTileView || view is MusicModeTileView ||
                view is AmbientEffectTileView
            ) view.isEnabled = online
        }
        listOf(lightPanel, musicPanel, ambientPanel, nightPanel, alarmPanel).forEach(::visit)
        // Labels are separate clickable TextViews, not part of the Button cases.
        ambientPresetLabels.values.forEach { it.isEnabled = online }
        ambientExtraPresetLabels.values.forEach { it.isEnabled = online }
        currentLimitSeek.isEnabled = online
        findViewById<Button>(R.id.resetDefaultsButton).isEnabled = online
        findViewById<Button>(R.id.eventsButton).isEnabled = online
        findViewById<Button>(R.id.sendRawButton).isEnabled = online
    }

    /**
     * Checks the gateway AND actual Nano UART status while UI is foreground.
     * A light GET never changes active light modes or saved settings.
     * Single executor serializes these probes after any pending user write.
     */
    private fun checkDeviceHeartbeat() {
        if (heartbeatInFlight || refreshInProgress) return
        heartbeatInFlight = true
        val alreadyOnline = deviceOnline
        val oldMode = latestMode

        worker.execute {
            var espResponded = false
            var nanoResponded = false
            try {
                val ping = if (alreadyOnline) api.pingSelected() else api.ping()
                espResponded = true
                val status = api.status()
                nanoResponded = true
                // Full state readback only on reconnect or externally changed mode.
                val snapshot = if (!alreadyOnline || oldMode != status.mode) {
                    readSnapshot()
                } else null
                runOnUiThread {
                    heartbeatInFlight = false
                    if (!heartbeatEnabled) return@runOnUiThread
                    lastConnectionHealth = DeviceConnectionHealth.ONLINE
                    lastEspFirmware = ping.firmware
                    lastEspRssi = ping.rssi
                    lastNetworkMode = ping.networkMode
                    if (!deviceOnline) {
                        deviceOnline = true
                        setDeviceControlsEnabled(true)
                    }
                    snapshot?.let(::renderSnapshot)
                    val label = if (ping.networkMode == "softap") {
                        "● Прямое подключение"
                    } else "● Онлайн"
                    setConnectionStatus(label, R.color.ardu_accent)
                }
            } catch (_: Exception) {
                val health = DeviceConnectionPolicy.classify(
                    espResponded, nanoResponded, settingsAvailable = false
                )
                runOnUiThread {
                    heartbeatInFlight = false
                    if (!heartbeatEnabled) return@runOnUiThread
                    if (deviceOnline || lastConnectionHealth != health) {
                        showUnavailableDeviceState()
                        lastConnectionHealth = health
                        setConnectionStatus(
                            when (health) {
                                DeviceConnectionHealth.ESP_ONLY -> "● Nano не отвечает"
                                DeviceConnectionHealth.SETTINGS_UNAVAILABLE -> "● Ошибка данных"
                                else -> "● Нет связи"
                            },
                            R.color.ardu_danger
                        )
                        setOperationStatus(
                            when (health) {
                                DeviceConnectionHealth.ESP_ONLY -> "ESP доступна, Nano не отвечает"
                                DeviceConnectionHealth.SETTINGS_UNAVAILABLE ->
                                    "ESP и Nano отвечают, но настройки не прочитаны"
                                else -> "Соединение с ARDU потеряно"
                            },
                            important = true
                        )
                    }
                }
            }
        }
    }

    private fun refreshDevice() {
        if (refreshInProgress) return
        refreshInProgress = true
        setConnectionStatus("Проверка…", R.color.ardu_text_secondary)
        refreshButton.isEnabled = false
        lightRefreshButton.isEnabled = false
        musicRefreshButton.isEnabled = false
        ambientRefreshButton.isEnabled = false
        nightRefreshButton.isEnabled = false
        alarmRefreshButton.isEnabled = false
        setOperationStatus("")

        worker.execute {
            var espResponded = false
            try {
                val ping = api.ping()
                espResponded = true
                lastEspFirmware = ping.firmware
                lastEspRssi = ping.rssi
                lastNetworkMode = ping.networkMode
                val snapshot = readSnapshot()
                val selectedAddress = api.selectedAddress()
                if (!selectedAddress.isNullOrBlank() &&
                    selectedAddress != "http://192.168.4.1") {
                    preferences().edit().putString(PREF_ADDRESS, selectedAddress).apply()
                }

                runOnUiThread {
                    val connectionLabel =
                        if (ping.networkMode == "softap") "● Прямое подключение"
                        else "● Онлайн"
                    refreshInProgress = false
                    lastConnectionHealth = DeviceConnectionHealth.ONLINE
                    deviceOnline = true
                    setDeviceControlsEnabled(true)
                    setConnectionStatus(connectionLabel, R.color.ardu_accent)
                    if (!selectedAddress.isNullOrBlank()) {
                        addressInput.setText(selectedAddress.removePrefix("http://"))
                    }
                    renderSnapshot(snapshot)
                    refreshButton.isEnabled = true
                    lightRefreshButton.isEnabled = true
                    musicRefreshButton.isEnabled = true
                    ambientRefreshButton.isEnabled = true
                    nightRefreshButton.isEnabled = true
                    alarmRefreshButton.isEnabled = true
                }
            } catch (error: Exception) {
                val health = DeviceConnectionPolicy.classify(espResponded, false)
                runOnUiThread {
                    refreshInProgress = false
                    showUnavailableDeviceState()
                    lastConnectionHealth = health
                    setConnectionStatus(
                        if (health == DeviceConnectionHealth.ESP_ONLY) {
                            "● Nano не отвечает"
                        } else "● Нет связи",
                        R.color.ardu_danger
                    )
                    setOperationStatus(error.message ?: "Ошибка подключения", important = true)
                    refreshButton.isEnabled = true
                    lightRefreshButton.isEnabled = true
                    musicRefreshButton.isEnabled = true
                    ambientRefreshButton.isEnabled = true
                    nightRefreshButton.isEnabled = true
                    alarmRefreshButton.isEnabled = true
                }
            }
        }
    }

    private fun readSnapshot(): Snapshot =
        Snapshot(api.status(), api.settings(), api.time())

    private fun runDeviceAction(
        label: String,
        onAcknowledged: (() -> Unit)? = null,
        action: () -> Unit
    ) {
        if (!UiSafetyPolicy.canSendCommand(deviceOnline)) {
            setOperationStatus("Нет связи с ARDU — команда не отправлена", important = true)
            return
        }
        setOperationStatus("$label…")
        worker.execute {
            // A successful POST response confirms the write; a later failed
            // GET must never be presented as a failed command or cause replay.
            try {
                action()
            } catch (error: Exception) {
                showError("$label — нет подтверждения команды", error)
                return@execute
            }

            val snapshot = try {
                readSnapshot()
            } catch (_: Exception) {
                null
            }
            runOnUiThread {
                onAcknowledged?.invoke()
                if (snapshot != null) {
                    renderSnapshot(snapshot)
                    setOperationStatus("$label: готово")
                } else {
                    // Do not present stale readback/slider values as authoritative.
                    // Foreground heartbeat restores the full snapshot on reconnect.
                    showUnavailableDeviceState()
                    setConnectionStatus("● Проверка состояния…", R.color.ardu_text_secondary)
                    setOperationStatus(
                        "$label: устройство подтвердило команду; состояние перечитывается",
                        important = true
                    )
                }
            }
        }
    }

    private fun renderSnapshot(snapshot: Snapshot) {
        rendering = true
        latestSettings = snapshot.settings
        latestMode = snapshot.status.mode

        modeText.text = modeTitle(snapshot.status.mode)
        timeText.text = if (snapshot.time.valid) {
            snapshot.time.time?.take(5) ?: "--:--"
        } else {
            "RTC —"
        }

        renderLight(snapshot)
        renderMusic(snapshot)
        renderAmbient(snapshot.settings)
        renderNight(snapshot.settings)
        renderAlarm(snapshot)
        renderService(snapshot)
        updateServiceMessageVisibility()

        rendering = false
    }

    private fun renderLight(snapshot: Snapshot) {
        val l = snapshot.settings.light
        lightStateText.text =
            "${if (snapshot.status.mode == "light") "Включён" else "Выключен"} • " +
            "${if (l.colorMode == "kelvin") "${l.kelvin} K" else "RGB"} • " +
            "${brightnessPercent(l.brightness)}%" +
            if (l.dirty) " • изменения не сохранены" else ""

        lightFocusDial.setValue(l.brightness)
        lightBrightnessSeek.setValue(l.brightness)
        lightKelvinSeek.setValue(l.kelvin)
        lightOverviewBrightnessText.text = "${brightnessPercent(l.brightness)}%"
        lightOverviewKelvinText.text = "${l.kelvin} K"
        val lightActive = snapshot.status.mode == "light"
        setChoiceState(lightOnButton, lightActive)
        setChoiceState(lightOffButton, !lightActive)
        setChoiceState(lightOverviewOnButton, lightActive)
        setChoiceState(lightOverviewOffButton, !lightActive)
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

        // A new calibration started by another controller supersedes local
        // dismissal, but Nano may keep "finished" true AFTER EEPROM save.
        if (clap.calibration.active && clapWizardDismissed) {
            setClapWizardDismissed(false)
        }
        val showClapWizard = ClapWizardUiPolicy.shouldShow(
            clap.calibration.active, clap.calibration.finished, clapWizardDismissed
        )
        clapCalibrationPanel.visibility = if (showClapWizard) View.VISIBLE else View.GONE
        if (showClapWizard) {
            clapCalibrationText.text =
                "Калибровка ${clap.calibration.goodPairs}/${clap.calibration.targetPairs}, " +
                "тишина=${clap.calibration.quietP99}, порог=${clap.calibration.suggestedThreshold}"
        }
        val hasAllPairs = clap.calibration.targetPairs > 0 &&
            clap.calibration.goodPairs >= clap.calibration.targetPairs
        clapSampleButton.isEnabled = showClapWizard &&
            clap.calibration.active && !hasAllPairs
        clapFinishButton.isEnabled = showClapWizard &&
            clap.calibration.active && hasAllPairs
        clapSaveButton.isEnabled = showClapWizard && clap.calibration.finished
    }

    private fun renderMusic(snapshot: Snapshot) {
        val settings = snapshot.settings
        val id = settings.music.selected
        val cfg = settings.music.modes[id] ?: return

        val musicActive = snapshot.status.mode == "music"
        musicModeText.text = if (musicActive) {
            musicModeTitle(id) + " • активно"
        } else {
            musicModeTitle(id) + " • выключено"
        }
        // Primary start button has a mint background; mint selected text is illegible.
        musicStartButton.isSelected = musicActive
        musicStartButton.setTextColor(getColor(R.color.ardu_on_accent))
        setChoiceState(musicStopButton, !musicActive)
        musicModeButtonMap.forEach { (modeId, tile) ->
            tile.isSelected = snapshot.status.mode == "music" && modeId == id
            tile.invalidate()
        }

        musicBrightnessSeek.setValue(cfg.brightness)
        musicBackgroundSeek.setValue(cfg.backgroundBrightness)
        musicSmoothingSeek.setValue(cfg.smoothing.coerceIn(5, 100))
        musicSensitivitySeek.setValue(cfg.sensitivity.coerceIn(50, 200))
        musicSpeedSeek.setValue(cfg.speed.coerceIn(1, 255))

        musicBrightnessText.text = "Эффект • ${brightnessPercent(cfg.brightness)}%"
        musicBackgroundText.text = "Фон • ${brightnessPercent(cfg.backgroundBrightness)}%"
        musicSmoothingText.text = "Плавность • ${cfg.smoothing}"
        musicSensitivityText.text = "Чувствительность • ${cfg.sensitivity}"
        musicSpeedText.text = "Скорость • ${cfg.speed}"

        updateMusicVisibility(id)

        musicSubmodeButtonMap.forEach { (submode, button) ->
            setChoiceState(button, submode == cfg.submode)
        }

        when (id) {
            "M02" -> {
                musicAuxSeek.configure(5, 200, SmartSliderView.VisualMode.ACCENT)
                musicAuxSeek.setValue(cfg.aux.coerceIn(5, 200))
            }
            "M09" -> {
                musicAuxSeek.configure(1, 255, SmartSliderView.VisualMode.ACCENT)
                musicAuxSeek.setValue(cfg.aux.coerceIn(1, 255))
                musicHueStartSeek.setValue(cfg.speed.coerceIn(0, 255))
                musicHueStartText.text = "Цвет • ${cfg.speed}"
            }
        }
        updateMusicAuxLabel(musicAuxSeek.value())

        val sys = settings.system
        musicCalibrationText.text =
            if (sys.audioCalibrated) {
                "DC ${sys.micDc} • VU ${sys.vuLowPass} • Spectrum ${sys.spectrumLowPass}"
            } else {
                "Микрофон не откалиброван"
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
        val p = settings.ambientPresets
        val selected = if (p.supported) p.selected else null
        val cfg = a.effects["F01"] ?: return
        val ambientActive = latestMode == "ambient"

        ambientEffectText.text = (
            if (selected != null) ambientSceneNames[selected] ?: selected
            else "Постоянный цвет"
        ) + if (ambientActive) " • активно" else " • выключено"
        setChoiceState(ambientOnButton, ambientActive)
        setChoiceState(ambientOffButton, !ambientActive)
        ambientEffectButtonMap.forEach { (_, tile) ->
            tile.isSelected = ambientActive && selected == null
            tile.invalidate()
        }
        setAmbientPresetSelection(if (ambientActive) selected else null)
        ambientPresetCapabilityText.text = if (p.supported) {
            "12 сцен доступны • параметры сохраняются в ARDU"
        } else {
            "Для атмосферных сцен нужны новые прошивки ESP и Nano"
        }
        ambientPresetButtons.values.forEach { it.isEnabled = p.supported }
        ambientExtraPresetButtons.values.forEach { it.isEnabled = p.supported }
        ambientPresetLabels.values.forEach { it.isEnabled = p.supported }
        ambientExtraPresetLabels.values.forEach { it.isEnabled = p.supported }

        ambientManualPanel.visibility = if (selected == null) View.VISIBLE else View.GONE
        ambientPresetControls.visibility = if (selected != null) View.VISIBLE else View.GONE
        if (selected != null) {
            val scene = p.scenes[selected]
            ambientPresetStatusText.text = ambientSceneNames[selected] ?: selected
            if (scene != null) {
                ambientPresetBrightnessSeek.setValue(scene.brightness)
                ambientPresetDynamicsSeek.setValue(scene.dynamics)
                ambientPresetBrightnessText.text =
                    "Яркость • ${brightnessPercent(scene.brightness)}%"
                ambientPresetDynamicsText.text = "Динамика • ${scene.dynamics}"
            }
        }
        // F01 still has its own hue/saturation/brightness.
        ambientHueSeek.setValue(cfg.hue)
        ambientBrightnessSeek.setValue(cfg.brightness)
        ambientHueText.text = "Цвет • ${cfg.hue}"
        ambientBrightnessText.text = "Яркость • ${brightnessPercent(cfg.brightness)}%"
        cfg.saturation?.let {
            ambientSaturationSeek.setValue(it)
            ambientSaturationText.text = "Насыщенность • ${brightnessPercent(it)}%"
        }
        ambientSaturationGroup.visibility = View.VISIBLE
        ambientSpeedGroup.visibility = View.GONE
        ambientRainbowGroup.visibility = View.GONE
        updateAmbientPreview()
    }

    private fun renderNight(settings: ArduSettings) {
        val n = settings.night

        nightEnabledSwitch.isChecked = n.enabled
        nightEnabledSwitch.text = if (n.enabled) "Ночник включён" else "Ночник выключен"
        nightHueSeek.setValue(n.hue)
        nightSaturationSeek.setValue(n.saturation)
        nightBrightnessSeek.setValue(n.brightness)
        nightScheduleSwitch.isChecked = n.scheduleEnabled
        nightOnInput.setText(n.scheduleOn)
        nightOffInput.setText(n.scheduleOff)

        nightHueText.text = "Цвет • ${n.hue}"
        nightSaturationText.text = "Насыщенность • ${brightnessPercent(n.saturation)}%"
        nightBrightnessText.text = "Яркость • ${brightnessPercent(n.brightness)}%"
        nightStateText.text = buildString {
            append(if (n.enabled) "Включён" else "Выключен")
            append(" • ${brightnessPercent(n.brightness)}%")
            if (n.scheduleEnabled) append(" • ${n.scheduleOn}–${n.scheduleOff}")
            else append(" • вручную")
        }
        updateNightPreview()
    }

    private fun renderAlarm(snapshot: Snapshot) {
        val a = snapshot.settings.alarm

        alarmEnabledSwitch.isChecked = a.enabled
        alarmHourInput.setText(a.hour.toString().padStart(2, '0'))
        alarmMinuteInput.setText(a.minute.toString().padStart(2, '0'))
        alarmFadeSeek.setValue(a.fadeMinutes)
        alarmBrightnessSeek.setValue(a.maxBrightness)
        alarmStartHueSeek.setValue(a.startHue)
        alarmEndHueSeek.setValue(a.endHue)

        alarmFadeText.text = "Длительность • ${a.fadeMinutes} мин"
        alarmBrightnessText.text = "Макс. яркость • ${brightnessPercent(a.maxBrightness)}%"
        alarmStartHueText.text = "Начало • ${a.startHue}"
        alarmEndHueText.text = "Финиш • ${a.endHue}"
        alarmStateText.text =
            "${if (a.enabled) "Включён" else "Выключен"} • " +
            String.format(Locale.US, "%02d:%02d", a.hour, a.minute) +
            if (a.dawnPhase != "idle") " • ${a.dawnPhase}" else ""

        alarmRtcText.text = if (snapshot.time.valid) {
            "RTC " + (snapshot.time.time?.take(5) ?: "--:--")
        } else {
            "Время устройства неизвестно"
        }
        stopDawnButton.isEnabled = UiSafetyPolicy.dawnCanBeStopped(a.dawnPhase)

        updateAlarmPreviews()
    }

    private fun renderService(snapshot: Snapshot) {
        val status = snapshot.status
        val sys = snapshot.settings.system
        currentLimitSeek.progress = sys.currentLimitMa
        currentLimitText.text = "Лимит тока: ${sys.currentLimitMa} мА"
        systemSummaryText.text = buildString {
            appendLine("ESP: $lastEspFirmware")
            appendLine(
                "Network: " + when (lastNetworkMode) {
                    "softap" -> "ARDU-DIRECT"
                    "station" -> "Home Wi-Fi"
                    else -> lastNetworkMode
                }
            )
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

    private fun musicModeTitle(id: String): String = when (id) {
        "M01" -> "VU градиент"
        "M02" -> "VU радуга"
        "M03" -> "5 полос"
        "M04" -> "3 полосы"
        "M05" -> "Частота"
        "M08" -> "Бегущие частоты"
        "M09" -> "Спектр"
        else -> id
    }

    private fun ambientEffectTitle(id: String): String = when (id) {
        "F01" -> "Постоянный цвет"
        "F02" -> "Плавная смена"
        "F03" -> "Бегущая радуга"
        else -> id
    }

    private fun setChoiceState(button: Button, selected: Boolean) {
        button.isSelected = selected
        button.setTextColor(
            getColor(if (selected) R.color.ardu_accent else R.color.ardu_text)
        )
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

    private fun bindSmartSlider(
        slider: SmartSliderView,
        min: Int,
        max: Int,
        mode: SmartSliderView.VisualMode,
        preview: (Int) -> Unit,
        commit: (Int) -> Unit
    ) {
        slider.configure(min, max, mode)
        slider.setListener(preview, commit)
    }

    private fun dp(value: Int): Int =
        (value * resources.displayMetrics.density).toInt()

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
        val message = label + ": " + (error.message ?: "ошибка")
        runOnUiThread { setOperationStatus(message, important = true) }
        // On a failed write, discard local previews and re-read Nano settings.
        worker.execute {
            try {
                val snapshot = readSnapshot()
                runOnUiThread {
                    deviceOnline = true
                    setDeviceControlsEnabled(true)
                    renderSnapshot(snapshot)
                    setOperationStatus(message, important = true)
                }
            } catch (_: Exception) {
                runOnUiThread {
                    showUnavailableDeviceState()
                    setConnectionStatus("● Нет связи", R.color.ardu_danger)
                    setOperationStatus(message, important = true)
                }
            }
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
        private const val PREF_SHOW_SERVICE_MESSAGES = "show_service_messages"
        private const val PREF_CLAP_WIZARD_DISMISSED = "clap_wizard_dismissed"
    }
}
