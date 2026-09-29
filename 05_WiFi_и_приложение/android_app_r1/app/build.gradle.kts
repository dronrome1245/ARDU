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
        versionCode = 1
        versionName = "0.1-r1"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
