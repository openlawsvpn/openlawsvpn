plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.openlawsvpn.poc"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.openlawsvpn.poc"
        minSdk = 26
        targetSdk = 34
        versionCode = 1
        versionName = "0.1.0-poc"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        viewBinding = true
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.appcompat:appcompat:1.7.0")
    implementation("com.google.android.material:material:1.12.0")
    implementation("androidx.constraintlayout:constraintlayout:2.1.4")
    // Chrome Custom Tabs
    implementation("androidx.browser:browser:1.8.0")
    // Coroutines for non-blocking server socket
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.8.1")
}
