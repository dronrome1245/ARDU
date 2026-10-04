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
        versionCode = 18
        versionName = "0.20.1-music-visual-hotfix-rc1"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}


dependencies {
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
}
