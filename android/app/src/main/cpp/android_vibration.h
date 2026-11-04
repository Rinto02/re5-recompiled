#ifndef RE5_ANDROID_VIBRATION_H
#define RE5_ANDROID_VIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/** Called by the SDK when the gamepad rumbles. Makes the Android phone vibrate
 *  with the given duration (ms) and intensity (0-100), if the user enabled it. */
void re5_android_vibrate(int durationMs, int intensity);

/** Returns true if the user enabled phone vibration in the launcher (VIBRATION_PHONE). */
bool re5_android_vibration_enabled(void);

/** Returns the vibration mode: "phone", "gamepad", or "both". */
const char* re5_android_vibration_mode(void);

#ifdef __cplusplus
}
#endif

#endif // RE5_ANDROID_VIBRATION_H
