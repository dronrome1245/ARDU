plugins {
    id("com.android.application")
}

android {
    namespace = "com.ardu.app"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.ardu.app"
        minSdk = 26
        targetSdk = 36
        versionCode = 4
        versionName = "0.4-l01-bright-kelvin-r1"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
