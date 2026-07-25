#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PLANKTON_COUNT    40
#define BUBBLE_COUNT      30
#define SCATTER_BUBBLE_MAX 24

#define BOTTOM_BUBBLE_COUNT 14

#define BOTTOM_JELLY_SCALE       0.5355f
#define BOTTOM_JELLY_BASE_SPEED  0.136f
#define BOTTOM_JELLY_TENTACLES   4
#define BOTTOM_JELLY_MAX_COUNT   6
#define CAUSTIC_COUNT     11

#define JELLY_COUNT        2
#define JELLY_TENTACLE_MAX 8

#define JELLY_DRIFTER_MAX_ACTIVE     BOTTOM_JELLY_MAX_COUNT

#define JELLY_DRIFTER_TILT_DEGREES  30.0f

#define FISH_COUNT          5

#define SCHOOL_COUNT_MAX     2
#define SCHOOL_FISH_MIN      5
#define SCHOOL_FISH_MAX     15
#define SCHOOL_FISH_CAP     (SCHOOL_COUNT_MAX * SCHOOL_FISH_MAX)

#define TURTLE_MIN_WAIT_FRAMES 28800
#define TURTLE_MAX_WAIT_FRAMES 43200

#define TURTLE_STROKE_CYCLE_FRAMES 180

#define TURTLE_BABY_COUNT 5
#define TURTLE_BABY_STROKE_CYCLE_FRAMES 90

#define SHARK_Y_MIN   20.0f
#define SHARK_Y_MAX   65.0f
#define SHARK_SPEED_MIN 0.35f
#define SHARK_SPEED_MAX 0.55f

#define HOUR_EVENT_CLEARING_FRAMES 600
#define HOUR_SCHOOL_FONT_COLS      3
#define HOUR_SCHOOL_FONT_ROWS      5
#define HOUR_SCHOOL_CELL_SIZE      13.0f
#define HOUR_SCHOOL_DIGIT_GAP      16.0f
#define HOUR_EVENT_MAX_FISH        60

#define HOUR_EVENT_PAUSE_DELAY 0xFFFFFFF0u

static const char* HOUR_DIGIT_FONT[11][HOUR_SCHOOL_FONT_ROWS] = {
    {"111","101","101","101","111"},
    {"010","110","010","010","111"},
    {"111","001","111","100","111"},
    {"111","001","111","001","111"},
    {"101","101","111","001","001"},
    {"111","100","111","001","111"},
    {"111","100","111","101","111"},
    {"111","001","010","010","010"},
    {"111","101","111","101","111"},
    {"111","101","111","001","111"},
    {"000","010","000","010","000"},
};
#define HOUR_SCHOOL_COLON_INDEX 10

#define ABYSS_AVG_WAIT_FRAMES   36000
#define ABYSS_WAIT_VARIANCE     12000
#define ABYSS_SPEED_MIN   0.468f
#define ABYSS_SPEED_MAX   0.684f
#define ABYSS_Y_MIN         38.0f
#define ABYSS_Y_MAX         62.0f

#define SAVE_PATH_RECORDS  "sdmc:/3ds/OceanClock/records.bin"
#define SAVE_PATH_SETTINGS "sdmc:/3ds/OceanClock/settings.bin"
#define SAVE_PATH_ALARM    "sdmc:/3ds/OceanClock/alarm.bin"
#define SAVE_DIR           "sdmc:/3ds/OceanClock"

#define THEME_TRANSITION_SPEED 0.05f
#define COLOR_TRANSITION_SPEED 0.08f

#define COMPILER_COLOR(r,g,b,a) (((u32)(a)<<24)|((u32)(b)<<16)|((u32)(g)<<8)|(u32)(r))

#define BEEP_SAMPLE_RATE   22050
#define BEEP_TONE_HZ       880.0f
#define BEEP_COUNT         5
#define BEEP_DURATION_MS   120
#define BEEP_GAP_MS        30
#define BEEP_VOLUME        0.75f
#define ALARM_REPEAT_PAUSE_FRAMES 120
#define ALARM_FLASH_SPEED  6.0f

typedef enum {
    SCREEN_MAIN,
    SCREEN_SETTINGS,
    SCREEN_RECORDS,
    SCREEN_CREDITS,
    SCREEN_ALARM,
    SCREEN_TIMER
} MenuScreen;

typedef enum {
    TIMER_IDLE,
    TIMER_RUNNING,
    TIMER_PAUSED
} TimerState;

typedef struct {
    u32 totalTimeSeconds;
    u32 savedJellyCount;

} AppRecords;

typedef struct {
    u32 clockColorIndex;
    u32 timeFormat24h;
    u32 dateFormat;
    u32 bgThemeIndex;
    u32 clockSizePreset;
    u32 clockMode;
    u32 gradientInverted;
    float clockOffsetX;
    float clockOffsetY;
} AppSettings;

typedef struct {
    u32 enabled;
    u32 hour;
    u32 minute;
} AppAlarm;

typedef struct {
    float x, y;
    float phase;
    float speed;
    float base;
    float size;
    float pulseFreq;
    float pulseAmp;
} Plankton;

typedef struct {
    float x, y;
    float speedY;
    float wobbleSpeed;
    float wobblePhase;
    float wobbleAmp;
    float alpha;
    float size;
} Bubble;

typedef struct {
    float x, y;
    float velX, velY;
    int   life;
    int   maxLife;
    float size;
} ScatterBubble;

typedef struct {
    float baseX;
    float topY;
    float length;
    float width;
    float swayPhase;
    float swaySpeed;
    float swayAmp;
    float alphaBase;
} Caustic;

typedef struct {
    float x, y;
    float speed;
    float direction;
    float baseY;
    float bobPhase;
    float bobSpeed;
    float bobAmp;
    float swimPhase;
    float scale;
    u32   entryDelayFrames;

} Fish;

typedef struct {
    bool  active;
    float centerX, centerY;
    float direction;
    float speed;
    float scale;
    int   fishCount;
    int   fishStartIdx;
    u32   entryDelayFrames;
    bool  willActivate;

} School;

typedef struct {
    float offsetX, offsetY;
    float bobPhase;
    float bobAmp;
    float swimPhase;
    float scale;
} SchoolFish;

typedef struct {
    bool  active;
    float x, y;
    float direction;
    float speed;
    float lanternPhase;
    float swimPhase;
    u32   waitFramesLeft;
} Anglerfish;

typedef struct {
    bool  active;
    float x, y;
    float direction;
    u32   waitFramesLeft;

    u32   strokeFrame;
    float speedBoost;

    u32   blinkFramesLeft;
    u32   blinkDurationLeft;

    float bobPhase;
} TurtleEvent;

typedef struct {
    float offsetX, offsetY;
    float scale;
    u32   strokeFrame;
} TurtleBaby;

typedef struct {
    bool  active;
    float x, y;
    float direction;
    float speed;
    float swimPhase;
    u32   waitFramesLeft;
} SharkEvent;

typedef enum {
    HOUR_EVENT_IDLE,
    HOUR_EVENT_CLEARING,
    HOUR_EVENT_SCHOOL_FORMATION
} HourEventPhase;

typedef struct {
    float offsetX, offsetY;
    float bobPhase;
    float swimPhase;
    float scale;
} HourSchoolFish;

typedef struct {
    HourEventPhase phase;
    int   schoolFishCount;
    HourSchoolFish schoolFish[HOUR_EVENT_MAX_FISH];
    float groupCenterX, groupCenterY;
    float groupDirection;
    float groupSpeed;
    bool  schoolOnTopScreen;

    int   schoolHourValue;

    u32   testClearingFramesLeft;
} HourEvent;

typedef enum {
    ABYSS_WAITING,
    ABYSS_ACTIVE
} AbyssPhase;

#define ABYSS_TRAIL_POINTS 18
#define ABYSS_TRAIL_STEP_PX 2.6f
typedef struct {
    AbyssPhase phase;
    u32 waitFramesLeft;

    float x, y;
    float direction;
    float speed;

    float bobPhase;
    float pulsePhase;
    float lightSweepPhase;
    float propellerAngle;

    float trailX[ABYSS_TRAIL_POINTS];
    float trailY[ABYSS_TRAIL_POINTS];
    int   trailCount;
    float trailDistAccum;
} AbyssEvent;

typedef struct {
    float x, y;
    float baseX, baseY;
    float driftPhaseX, driftPhaseY;
    float driftSpeedX, driftSpeedY;
    float driftAmpX, driftAmpY;
    float pulsePhase;
    float pulseSpeed;
    float scale;
    int   tentacleCount;
    float tentaclePhase[JELLY_TENTACLE_MAX];
    float tentacleLenVar[JELLY_TENTACLE_MAX];
} Jellyfish;

typedef struct {
    bool  active;
    float x, y;
    float direction;
    float speedX;
    float speedY;
    u32   waitFramesLeft;

    float pulsePhase;
    float pulseSpeed;
    float scale;
    int   tentacleCount;
    float tentaclePhase[4];
    float tentacleLenVar[4];

    float thrustBoost;
    bool  wasContracting;

    float lastDirection;
    int   sameDirectionStreak;

    int   arrivalBurstLeft;
    int   arrivalBurstCooldown;

    float tiltPhase, tiltSpeed;

    int   hesitationCooldown;
    int   hesitationFramesLeft;
    float hesitationFactor;

    float temperament;
} JellyDrifter;

typedef struct {
    float x, y;
    float dirX, dirY;
    float targetDirX, targetDirY;

    float pulsePhase, pulseSpeed;
    float tiltPhase, tiltSpeed;

    bool  hasEnteredScreen;

    bool  enteredFromRight;
    float tentaclePhase[4];
    float tentacleLenVar[4];
    bool  isLaunching;

    float scale;

    float temperament;

} BottomJelly;

typedef struct {
    float topR, topG, topB;
    float botR, botG, botB;
    u32 textMenuColor;
    u32 bubbleColor;
    const char* name;
} DepthTheme;

static const DepthTheme depthThemes[5] = {
    {3.0f,  10.0f, 22.0f, 10.0f,  35.0f, 55.0f, COMPILER_COLOR(95,  210, 230, 255), COMPILER_COLOR(150, 230, 240, 255), "Twilight Zone"},
    {1.0f,  3.0f,  10.0f, 4.0f,   14.0f, 28.0f, COMPILER_COLOR(60,  160, 190, 255), COMPILER_COLOR(110, 200, 210, 255), "Midnight Zone"},
    {0.0f,  4.0f,  6.0f,  2.0f,   18.0f, 20.0f, COMPILER_COLOR(70,  220, 170, 255), COMPILER_COLOR(120, 235, 200, 255), "Bioluminescent Drift"},
    {8.0f,  2.0f,  2.0f,  30.0f,  8.0f,  6.0f,  COMPILER_COLOR(235, 110, 50,  255), COMPILER_COLOR(250, 150, 70,  255), "Volcanic Vent"},
    {0.0f,  0.0f,  1.0f,  1.0f,   3.0f,  5.0f,  COMPILER_COLOR(80,  90,  110, 255), COMPILER_COLOR(100, 115, 140, 255), "Abyssal Black"}
};

static Plankton planktons[PLANKTON_COUNT];
static Bubble   bubbles[BUBBLE_COUNT];
static ScatterBubble scatterBubbles[SCATTER_BUBBLE_MAX];
static Bubble   bottomBubbles[BOTTOM_BUBBLE_COUNT];
static BottomJelly bottomJellies[BOTTOM_JELLY_MAX_COUNT];
static int bottomJellyCount = 0;
static Jellyfish jellies[JELLY_COUNT];
static JellyDrifter jellyDrifters[JELLY_DRIFTER_MAX_ACTIVE];

#define PENDING_ARRIVAL_MAX 6
static float pendingArrivalX[PENDING_ARRIVAL_MAX];
static float pendingArrivalTiltPhase[PENDING_ARRIVAL_MAX];
static float pendingArrivalTiltSpeed[PENDING_ARRIVAL_MAX];
static float pendingArrivalScale[PENDING_ARRIVAL_MAX];
static float pendingArrivalTemperament[PENDING_ARRIVAL_MAX];
static int   pendingArrivalCount = 0;
static Caustic   caustics[CAUSTIC_COUNT];
static Fish      fishes[FISH_COUNT];
static School     schools[SCHOOL_COUNT_MAX];
static SchoolFish schoolFish[SCHOOL_FISH_CAP];
static Anglerfish anglerfish;
static TurtleEvent turtleEvent;
static TurtleBaby turtleBabies[TURTLE_BABY_COUNT];
static SharkEvent sharkEvent;
static HourEvent hourEvent;
static AbyssEvent abyssEvent;

static float currentTopR = 3.0f,  currentTopG = 10.0f, currentTopB = 22.0f;
static float currentBotR = 10.0f, currentBotG = 35.0f, currentBotB = 55.0f;
static float currentClockR = 255.0f, currentClockG = 255.0f, currentClockB = 255.0f;
static float currentThemeTextR = 95.0f, currentThemeTextG = 210.0f, currentThemeTextB = 230.0f;

static bool settingsDirty = false;
static bool alarmDirty    = false;

static MenuScreen currentScreen = SCREEN_MAIN;
static AppRecords records  = {0};
static AppSettings settings = {11, 1, 1, 0, 1, 0, 0, 0.0f, 0.0f};
static AppAlarm    alarmCfg = {0, 0, 0};
static u32 sessionFrames = 0;

static bool lowerScreenOff = false;

static s16* beepBuffer = NULL;
static u32  beepBufferSamples = 0;
static ndspWaveBuf beepWaveBuf;
static bool audioReady = false;

static u32  alarmEditHour   = 0;
static u32  alarmEditMinute = 0;
static bool alarmEditPM     = false;

static bool alarmRinging       = false;
static int  alarmLastTrigHour  = -1;
static int  alarmLastTrigMin   = -1;
static u32  alarmRepeatTimer   = 0;
static float alarmFlashPhase   = 0.0f;

static TimerState timerState        = TIMER_IDLE;
static u32 timerTotalSeconds        = 0;
static u32 timerRemainingSeconds    = 0;
static u32 timerFrameAccumulator    = 0;

static u32 timerEditHour   = 0;
static u32 timerEditMinute = 0;
static u32 timerEditSecond = 0;

static C2D_TextBuf staticBuf;
typedef struct {
    C2D_Text title, hintMove, hintCenter, hintSelect, hintSize, hintStart, hintOff, btnSettings, btnRecords, btnOff;
    C2D_Text btnBack;
    C2D_Text setTitle, btnBackSet, btnResetSet, btnCredits, btnAlarmSet, btnTimerSet;
    C2D_Text credTitle, credLine1, credLine2, credLine3, credLine4, btnBackCred;
    C2D_Text alarmTitle, alarmHourLabel, alarmMinLabel, btnAlarmBack, btnAlarmSetConfirm, btnAlarmClear;
    C2D_Text timerTitle, timerHourLabel, timerMinLabel, timerSecLabel, btnTimerBack;
    C2D_Text btnTimerStart, btnTimerPause, btnTimerResume, btnTimerStop, btnTimerReset;
} StaticTexts;
static StaticTexts ui;

static u32 clockPresets[15];
static const char* presetNames[16] = {
    "Pure White", "Neon Blue",    "Emerald Green", "Blood Moon Red", "Cyber Yellow",
    "Hot Pink",   "Soft Cyan",    "Amethyst",      "Orange Fox",     "Mint Green",
    "Seafoam",    "Deep Turquoise", "Pearl Shell",  "Soft Coral",     "Abyssal Pale Blue",
    "Rainbow Dreams"
};

static void initColorPresets() {
    clockPresets[0] = C2D_Color32(255, 255, 255, 255);
    clockPresets[1] = C2D_Color32(  0, 191, 255, 255);
    clockPresets[2] = C2D_Color32( 50, 205,  50, 255);
    clockPresets[3] = C2D_Color32(230,  30,  30, 255);
    clockPresets[4] = C2D_Color32(255, 215,   0, 255);
    clockPresets[5] = C2D_Color32(255,  20, 147, 255);
    clockPresets[6] = C2D_Color32(128, 255, 212, 255);
    clockPresets[7] = C2D_Color32(153,  50, 204, 255);
    clockPresets[8] = C2D_Color32(255,  69,   0, 255);
    clockPresets[9] = C2D_Color32(173, 255,  47, 255);

    clockPresets[10] = C2D_Color32(160, 235, 210, 255);
    clockPresets[11] = C2D_Color32( 60, 175, 185, 255);
    clockPresets[12] = C2D_Color32(225, 235, 230, 255);
    clockPresets[13] = C2D_Color32(235, 160, 145, 255);
    clockPresets[14] = C2D_Color32(150, 190, 220, 255);
}

static void initStaticTexts() {
    staticBuf = C2D_TextBufNew(1024);
    #define PT(dst, str) C2D_TextParse(dst, staticBuf, str); C2D_TextOptimize(dst)

    PT(&ui.title,       "OceanClock");
    PT(&ui.hintMove,    "Press L + Analog to Move Clock");
    PT(&ui.hintCenter,  "Press L + A to Center Clock");
    PT(&ui.hintSelect,  "Press SELECT to Cycle Display Modes");
    PT(&ui.hintSize,    "Press D-Pad Left/Right to Resize Clock");
    PT(&ui.hintStart,   "Press START to Close Application");
    PT(&ui.hintOff,     "Tap 'Screen Off' to Disable the Lower Screen");
    PT(&ui.btnSettings, "Settings");
    PT(&ui.btnRecords,  "Manual");
    PT(&ui.btnOff,      "Screen Off");

    PT(&ui.btnBack,     "Back");

    PT(&ui.setTitle,    "SETTINGS");
    PT(&ui.btnBackSet,  "Back");
    PT(&ui.btnResetSet, "Reset");
    PT(&ui.btnCredits,  "Credits");
    PT(&ui.btnAlarmSet, "Alarm");
    PT(&ui.btnTimerSet, "Timer");

    PT(&ui.credTitle,   "OceanClock Credits");
    PT(&ui.credLine1,   "Developed by: Michele P.");
    PT(&ui.credLine2,   "A Relaxing OceanClock App");
    PT(&ui.credLine3,   "Send feedbacks to Michelep3ds@gmail.com");
    PT(&ui.credLine4,   "Version 1.00");
    PT(&ui.btnBackCred, "Back");

    PT(&ui.alarmTitle,        "ALARM");
    PT(&ui.alarmHourLabel,    "Hour");
    PT(&ui.alarmMinLabel,     "Minute");
    PT(&ui.btnAlarmBack,      "Back");
    PT(&ui.btnAlarmSetConfirm,"SET");
    PT(&ui.btnAlarmClear,     "Clear");

    PT(&ui.timerTitle,      "TIMER");
    PT(&ui.timerHourLabel,  "Hour");
    PT(&ui.timerMinLabel,   "Minute");
    PT(&ui.timerSecLabel,   "Second");
    PT(&ui.btnTimerBack,    "Back");
    PT(&ui.btnTimerStart,   "Start");
    PT(&ui.btnTimerPause,   "Pause");
    PT(&ui.btnTimerResume,  "Resume");
    PT(&ui.btnTimerStop,    "Stop");
    PT(&ui.btnTimerReset,   "Reset");
    #undef PT
}

static void saveRecords() {
    FILE* f = fopen(SAVE_PATH_RECORDS, "wb");
    if (f) { fwrite(&records, sizeof(AppRecords), 1, f); fclose(f); }
}

static void loadRecords() {
    FILE* f = fopen(SAVE_PATH_RECORDS, "rb");
    if (f) { fread(&records, sizeof(AppRecords), 1, f); fclose(f); }
    else   { records = (AppRecords){0}; }
}

static void saveSettings() {
    FILE* f = fopen(SAVE_PATH_SETTINGS, "wb");
    if (f) { fwrite(&settings, sizeof(AppSettings), 1, f); fclose(f); }
    settingsDirty = false;
}

static void loadSettings() {
    FILE* f = fopen(SAVE_PATH_SETTINGS, "rb");
    if (f) { fread(&settings, sizeof(AppSettings), 1, f); fclose(f); }
    else   { settings = (AppSettings){11, 1, 1, 0, 1, 0, 0, 0.0f, 0.0f}; }
}

static void saveAlarm() {
    FILE* f = fopen(SAVE_PATH_ALARM, "wb");
    if (f) { fwrite(&alarmCfg, sizeof(AppAlarm), 1, f); fclose(f); }
}

static void loadAlarm() {
    FILE* f = fopen(SAVE_PATH_ALARM, "rb");
    if (f) { fread(&alarmCfg, sizeof(AppAlarm), 1, f); fclose(f); }
    else   { alarmCfg = (AppAlarm){0, 0, 0}; }
}

static bool initAudio() {
    if (ndspInit() != 0) return false;

    u32 samplesPerBeep = (BEEP_SAMPLE_RATE * BEEP_DURATION_MS) / 1000;
    u32 samplesPerGap  = (BEEP_SAMPLE_RATE * BEEP_GAP_MS)      / 1000;
    beepBufferSamples  = (samplesPerBeep + samplesPerGap) * BEEP_COUNT;

    beepBuffer = (s16*)linearAlloc(beepBufferSamples * sizeof(s16));
    if (!beepBuffer) { ndspExit(); return false; }

    u32 pos = 0;
    for (int b = 0; b < BEEP_COUNT; b++) {

        for (u32 i = 0; i < samplesPerBeep; i++) {
            float t = (float)i / (float)BEEP_SAMPLE_RATE;
            float sample = sinf(2.0f * M_PI * BEEP_TONE_HZ * t);

            u32 fadeSamples = BEEP_SAMPLE_RATE * 5 / 1000;
            float env = 1.0f;
            if (i < fadeSamples) env = (float)i / (float)fadeSamples;
            else if (i > samplesPerBeep - fadeSamples) env = (float)(samplesPerBeep - i) / (float)fadeSamples;

            beepBuffer[pos++] = (s16)(sample * env * BEEP_VOLUME * 32767.0f);
        }

        for (u32 i = 0; i < samplesPerGap; i++) {
            beepBuffer[pos++] = 0;
        }
    }

    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)BEEP_SAMPLE_RATE);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);

    memset(&beepWaveBuf, 0, sizeof(beepWaveBuf));
    beepWaveBuf.data_vaddr = beepBuffer;
    beepWaveBuf.nsamples   = beepBufferSamples;
    beepWaveBuf.looping    = false;

    audioReady = true;
    return true;
}

static void exitAudio() {
    if (!audioReady) return;
    ndspChnWaveBufClear(0);
    if (beepBuffer) { linearFree(beepBuffer); beepBuffer = NULL; }
    ndspExit();
    audioReady = false;
}

static void playTestBeep() {
    if (!audioReady) return;
    ndspChnWaveBufClear(0);
    DSP_FlushDataCache(beepBuffer, beepBufferSamples * sizeof(s16));
    beepWaveBuf.status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(0, &beepWaveBuf);
}

static void startAlarmRinging() {
    alarmRinging     = true;
    alarmRepeatTimer = 0;
    alarmFlashPhase  = 0.0f;
    lowerScreenOff   = false;
}

static void stopAlarmRinging() {
    alarmRinging     = false;
    alarmCfg.enabled = 0;
    alarmDirty       = true;
    if (audioReady) ndspChnWaveBufClear(0);
}

static void updateAlarmRinging() {
    if (!alarmRinging) return;

    if (alarmRepeatTimer == 0) {
        playTestBeep();

        u32 samplesPerBeep = (BEEP_SAMPLE_RATE * BEEP_DURATION_MS) / 1000;
        u32 samplesPerGap  = (BEEP_SAMPLE_RATE * BEEP_GAP_MS)      / 1000;
        u32 groupMs        = ((samplesPerBeep + samplesPerGap) * BEEP_COUNT * 1000) / BEEP_SAMPLE_RATE;
        u32 groupFrames    = (groupMs * 60) / 1000;
        alarmRepeatTimer   = groupFrames + ALARM_REPEAT_PAUSE_FRAMES;
    }
    alarmRepeatTimer--;

    alarmFlashPhase += ALARM_FLASH_SPEED * (1.0f / 60.0f);
}

static void updateTimer() {
    if (timerState != TIMER_RUNNING) return;

    timerFrameAccumulator++;
    if (timerFrameAccumulator >= 60) {
        timerFrameAccumulator = 0;
        if (timerRemainingSeconds > 0) {
            timerRemainingSeconds--;
        }
        if (timerRemainingSeconds == 0) {
            timerState = TIMER_IDLE;
            startAlarmRinging();
        }
    }
}

static void initSingleBubble(int i, bool randomY) {
    bubbles[i].x            = rand() % 400;
    bubbles[i].y            = randomY ? (rand() % 240) : 245.0f;
    bubbles[i].speedY       = 0.3f + (rand() % 100) / 150.0f;
    bubbles[i].wobbleSpeed  = 0.03f + (rand() % 100) / 2000.0f;
    bubbles[i].wobblePhase  = rand() % 360;
    bubbles[i].wobbleAmp    = 0.5f + (rand() % 100) / 80.0f;
    bubbles[i].alpha        = 0.35f + (rand() % 100) / 250.0f;
    bubbles[i].size         = 0.8f + (rand() % 100) / 70.0f;
}

static void spawnScatterBubbles(float x, float y, int count) {
    int spawned = 0;
    for (int i = 0; i < SCATTER_BUBBLE_MAX && spawned < count; i++) {
        if (scatterBubbles[i].life > 0) continue;

        scatterBubbles[i].x       = x + ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
        scatterBubbles[i].y       = y + ((rand() % 100) / 100.0f - 0.5f) * 4.0f;
        scatterBubbles[i].velX    = ((rand() % 100) / 100.0f - 0.5f) * 0.3f;
        scatterBubbles[i].velY    = -(0.15f + (rand() % 100) / 400.0f);
        scatterBubbles[i].maxLife = 20 + rand() % 21;
        scatterBubbles[i].life    = scatterBubbles[i].maxLife;
        scatterBubbles[i].size    = 0.6f + (rand() % 100) / 200.0f;
        spawned++;
    }
}

static void updateAndDrawScatterBubbles() {
    for (int i = 0; i < SCATTER_BUBBLE_MAX; i++) {
        if (scatterBubbles[i].life <= 0) continue;

        scatterBubbles[i].life--;
        scatterBubbles[i].x += scatterBubbles[i].velX;
        scatterBubbles[i].y += scatterBubbles[i].velY;

        scatterBubbles[i].velX *= 0.96f;

        float lifeFrac = (float)scatterBubbles[i].life / (float)scatterBubbles[i].maxLife;
        u8 alpha = (u8)(lifeFrac * 180.0f);
        if (alpha <= 0) continue;

        u32 fillColor = C2D_Color32(220, 235, 245, (u8)(alpha * 0.5f));
        u32 ringColor = C2D_Color32(240, 248, 255, alpha);
        C2D_DrawCircleSolid(scatterBubbles[i].x, scatterBubbles[i].y, 0.0f, scatterBubbles[i].size, fillColor);
        C2D_DrawCircleSolid(scatterBubbles[i].x - scatterBubbles[i].size * 0.3f,
                             scatterBubbles[i].y - scatterBubbles[i].size * 0.3f,
                             0.0f, scatterBubbles[i].size * 0.35f, ringColor);
    }
}

static void initSingleBottomBubble(int i, bool randomY) {
    bottomBubbles[i].x            = rand() % 320;
    bottomBubbles[i].y            = randomY ? (rand() % 240) : 245.0f;
    bottomBubbles[i].speedY       = 0.3f + (rand() % 100) / 150.0f;
    bottomBubbles[i].wobbleSpeed  = 0.03f + (rand() % 100) / 2000.0f;
    bottomBubbles[i].wobblePhase  = rand() % 360;
    bottomBubbles[i].wobbleAmp    = 0.5f + (rand() % 100) / 80.0f;
    bottomBubbles[i].alpha        = 0.35f + (rand() % 100) / 250.0f;
    bottomBubbles[i].size         = 0.8f + (rand() % 100) / 70.0f;
}

static void spawnBottomJelly() {
    if (bottomJellyCount >= BOTTOM_JELLY_MAX_COUNT) return;

    BottomJelly* j = &bottomJellies[bottomJellyCount];

    bool enterFromRight = (rand() % 2 == 0);
    j->x = enterFromRight ? 340.0f : -20.0f;
    j->enteredFromRight = enterFromRight;

    j->y = 80.0f + rand() % 80;
    j->hasEnteredScreen = false;
    j->isLaunching = false;

    bool spawnsOnLeft = (j->x < 160.0f);
    float horizontalSign = spawnsOnLeft ? 1.0f : -1.0f;
    float angleAboveHorizontal = (15.0f + (float)(rand() % 61)) * 3.14159265f / 180.0f;
    j->dirX = j->targetDirX = horizontalSign * cosf(angleAboveHorizontal);
    j->dirY = j->targetDirY = -sinf(angleAboveHorizontal);

    j->temperament = (rand() % 1000) / 1000.0f;
    j->pulsePhase  = (float)(rand() % 360);
    j->pulseSpeed  = 0.45f + 0.5f * j->temperament + (rand() % 100) / 500.0f;
    j->tiltPhase = (float)(rand() % 360);
    j->tiltSpeed = 0.15f + (rand() % 100) / 500.0f;

    j->scale = BOTTOM_JELLY_SCALE * (0.85f + (rand() % 100) / 660.0f);

    for (int t = 0; t < BOTTOM_JELLY_TENTACLES; t++) {
        j->tentaclePhase[t]  = (float)t * 0.35f + (rand() % 60) / 100.0f;
        j->tentacleLenVar[t] = 0.78f + (rand() % 100) / 200.0f;
    }

    bottomJellyCount++;
}

static void spawnOrFreeBottomJelly() {
    if (bottomJellyCount < BOTTOM_JELLY_MAX_COUNT) {
        spawnBottomJelly();
        return;
    }

    int candidates[BOTTOM_JELLY_MAX_COUNT];
    int candidateCount = 0;
    for (int i = 0; i < bottomJellyCount; i++) {
        if (!bottomJellies[i].isLaunching) {
            candidates[candidateCount++] = i;
        }
    }
    if (candidateCount > 0) {
        int chosen = candidates[rand() % candidateCount];
        bottomJellies[chosen].isLaunching = true;
    }
}

static bool tryLaunchBottomJellyAt(float targetX, float targetY) {

    float bodyRadius = 16.0f * BOTTOM_JELLY_SCALE * 1.3f;
    float bodyRadiusSq = bodyRadius * bodyRadius;

    for (int i = 0; i < bottomJellyCount; i++) {
        BottomJelly* j = &bottomJellies[i];
        if (j->isLaunching) continue;

        float dx = targetX - j->x;
        float dy = targetY - j->y;
        if (dx*dx + dy*dy <= bodyRadiusSq) {
            j->isLaunching = true;
            return true;
        }
    }
    return false;
}

static void setBottomJellyTarget(float targetX, float targetY) {
    if (bottomJellyCount == 0) return;

    int closestIdx = 0;
    float closestDistSq = 1e18f;
    for (int i = 0; i < bottomJellyCount; i++) {
        float dx = targetX - bottomJellies[i].x;
        float dy = targetY - bottomJellies[i].y;
        float distSq = dx*dx + dy*dy;
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestIdx = i;
        }
    }

    BottomJelly* j = &bottomJellies[closestIdx];
    float dx = targetX - j->x;
    float dy = targetY - j->y;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist > 0.01f) {
        j->targetDirX = dx / dist;
        j->targetDirY = dy / dist;
    }
}

static void initSingleJellyfish(int i, float baseX, float baseY, float scale, float pulseSpeed, int tentacleCount) {
    jellies[i].x = jellies[i].baseX = baseX;
    jellies[i].y = jellies[i].baseY = baseY;
    jellies[i].driftPhaseX = (float)(rand() % 360);
    jellies[i].driftPhaseY = (float)(rand() % 360);
    jellies[i].driftSpeedX = 0.10f + (rand() % 100) / 1000.0f;
    jellies[i].driftSpeedY = 0.07f + (rand() % 100) / 1200.0f;
    jellies[i].driftAmpX   = 65.0f + (rand() % 100) / 4.0f;
    jellies[i].driftAmpY   = 45.0f + (rand() % 100) / 5.0f;
    jellies[i].pulsePhase  = (float)(rand() % 360);
    jellies[i].pulseSpeed  = pulseSpeed;
    jellies[i].scale       = scale;
    jellies[i].tentacleCount = tentacleCount;
    for (int t = 0; t < tentacleCount; t++) {
        jellies[i].tentaclePhase[t]  = (float)t * 0.35f + (rand() % 60) / 100.0f;

        jellies[i].tentacleLenVar[t] = 0.78f + (rand() % 100) / 200.0f;
    }
}

static void initJellyDrifterSlot(int idx) {
    jellyDrifters[idx].active = false;
}

static void spawnJellyDrifterFromArrival(int idx, float xPos, float tiltPhase, float tiltSpeed, float scale, float temperament) {
    JellyDrifter* d = &jellyDrifters[idx];
    d->active = true;

    d->direction = (xPos < 200.0f) ? 1.0f : -1.0f;
    if (d->direction == d->lastDirection) {
        d->sameDirectionStreak++;
    } else {
        d->sameDirectionStreak = 1;
    }
    d->lastDirection = d->direction;

    d->speedX = 0.10f + (rand() % 100) / 700.0f;
    d->speedY = 0.0338f + (rand() % 100) / 1333.0f;

    d->y = 255.0f;

    d->x = xPos;

    d->pulsePhase = (float)(rand() % 360);
    d->pulseSpeed = 0.55f + (rand() % 100) / 250.0f;

    d->scale      = scale;
    d->tentacleCount = 3 + rand() % 2;

    d->thrustBoost    = 1.0f;
    d->wasContracting = false;

    d->tiltPhase = tiltPhase;
    d->tiltSpeed = tiltSpeed;

    d->arrivalBurstLeft     = 3;
    d->arrivalBurstCooldown = 0;

    d->temperament = temperament;

    d->hesitationCooldown   = (int)(400 + 2600 * d->temperament) + rand() % 301;
    d->hesitationFramesLeft = 0;
    d->hesitationFactor     = 1.0f;

    for (int t = 0; t < d->tentacleCount; t++) {
        d->tentaclePhase[t]  = (float)t * 0.35f + (rand() % 60) / 100.0f;
        d->tentacleLenVar[t] = 0.78f + (rand() % 100) / 200.0f;
    }
}

static void processPendingJellyArrivals() {
    if (pendingArrivalCount == 0) return;

    for (int i = 0; i < JELLY_DRIFTER_MAX_ACTIVE && pendingArrivalCount > 0; i++) {
        if (!jellyDrifters[i].active) {
            spawnJellyDrifterFromArrival(i, pendingArrivalX[0], pendingArrivalTiltPhase[0], pendingArrivalTiltSpeed[0], pendingArrivalScale[0], pendingArrivalTemperament[0]);

            for (int k = 1; k < pendingArrivalCount; k++) {
                pendingArrivalX[k-1]           = pendingArrivalX[k];
                pendingArrivalTiltPhase[k-1]   = pendingArrivalTiltPhase[k];
                pendingArrivalTiltSpeed[k-1]   = pendingArrivalTiltSpeed[k];
                pendingArrivalScale[k-1]       = pendingArrivalScale[k];
                pendingArrivalTemperament[k-1] = pendingArrivalTemperament[k];
            }
            pendingArrivalCount--;
        }
    }
}

static void initSingleFish(int i, bool randomX) {

    fishes[i].direction = (rand() % 2 == 0) ? 1.0f : -1.0f;
    if (randomX) {
        fishes[i].x = rand() % 400;
    } else {

        fishes[i].x = (fishes[i].direction > 0.0f) ? -30.0f : 430.0f;
    }
    fishes[i].baseY    = 30.0f + rand() % 170;
    fishes[i].y        = fishes[i].baseY;
    fishes[i].speed     = 0.35f + (rand() % 100) / 180.0f;
    fishes[i].bobPhase  = (float)(rand() % 360);
    fishes[i].bobSpeed  = 0.6f + (rand() % 100) / 150.0f;
    fishes[i].bobAmp    = 4.0f + (rand() % 100) / 12.0f;
    fishes[i].swimPhase = (float)(rand() % 360);
    fishes[i].scale     = 0.7f + (rand() % 100) / 130.0f;
}

static void initSingleSchool(int idx, bool randomX) {
    School* s = &schools[idx];
    s->active     = true;
    s->direction  = (rand() % 2 == 0) ? 1.0f : -1.0f;
    s->speed       = 0.30f + (rand() % 100) / 220.0f;
    s->scale        = 0.35f + (rand() % 100) / 250.0f;
    s->fishCount    = SCHOOL_FISH_MIN + rand() % (SCHOOL_FISH_MAX - SCHOOL_FISH_MIN + 1);
    s->fishStartIdx = idx * SCHOOL_FISH_MAX;

    s->centerY = 25.0f + rand() % 150;
    if (randomX) {
        s->centerX = rand() % 400;
    } else {

        s->centerX = (s->direction > 0.0f) ? -60.0f : 460.0f;
    }

    for (int k = 0; k < s->fishCount; k++) {
        SchoolFish* sf = &schoolFish[s->fishStartIdx + k];
        float spreadX = 22.0f + (rand() % 100) / 4.0f;
        float spreadY = 10.0f + (rand() % 100) / 8.0f;

        float rx = ((rand() % 200) - 100 + (rand() % 200) - 100) / 200.0f;
        float ry = ((rand() % 200) - 100 + (rand() % 200) - 100) / 200.0f;
        sf->offsetX   = rx * spreadX;
        sf->offsetY   = ry * spreadY;
        sf->bobPhase  = (float)(rand() % 360);
        sf->bobAmp    = 2.0f + (rand() % 100) / 30.0f;
        sf->swimPhase = (float)(rand() % 360);
        sf->scale     = s->scale * (0.85f + (rand() % 100) / 330.0f);
    }
}

static void initAnglerfish() {
    anglerfish.active = false;
}

static void spawnAnglerfish() {
    anglerfish.active       = true;
    anglerfish.direction    = (rand() % 2 == 0) ? 1.0f : -1.0f;
    anglerfish.speed         = 0.18f + (rand() % 100) / 400.0f;
    anglerfish.y              = 195.0f + rand() % 30;
    anglerfish.x              = (anglerfish.direction > 0.0f) ? -40.0f : 440.0f;
    anglerfish.lanternPhase   = (float)(rand() % 360);
    anglerfish.swimPhase      = (float)(rand() % 360);
}

static void initTurtleEvent() {
    turtleEvent.active         = false;
    turtleEvent.waitFramesLeft = TURTLE_MIN_WAIT_FRAMES +
        (rand() % (TURTLE_MAX_WAIT_FRAMES - TURTLE_MIN_WAIT_FRAMES + 1));
}

static void spawnTurtleEvent() {
    turtleEvent.active       = true;

    bool lowBand = (rand() % 2 == 0);
    if (lowBand) {
        turtleEvent.y         = 180.0f + rand() % 30;
        turtleEvent.direction = 1.0f;
    } else {
        turtleEvent.y         = 35.0f + rand() % 11;
        turtleEvent.direction = -1.0f;
    }
    turtleEvent.x            = (turtleEvent.direction > 0.0f) ? -40.0f : 440.0f;
    turtleEvent.strokeFrame  = 0;
    turtleEvent.speedBoost   = 1.0f;
    turtleEvent.blinkFramesLeft   = 60 + rand() % 120;
    turtleEvent.blinkDurationLeft = 0;
    turtleEvent.bobPhase = (float)(rand() % 360);

    static const float babyOffsetX[TURTLE_BABY_COUNT] = { -30.0f, -38.0f, -46.0f, -34.0f, -50.0f };
    static const float babyOffsetY[TURTLE_BABY_COUNT] = { -12.0f,   6.0f,  -4.0f,  16.0f,   2.0f };
    for (int i = 0; i < TURTLE_BABY_COUNT; i++) {
        turtleBabies[i].offsetX     = babyOffsetX[i];
        turtleBabies[i].offsetY     = babyOffsetY[i];
        turtleBabies[i].scale       = 0.35f + (rand() % 100) / 250.0f;
        turtleBabies[i].strokeFrame = rand() % TURTLE_BABY_STROKE_CYCLE_FRAMES;
    }
}

static void initSharkEvent() {
    sharkEvent.active = false;
}

static void spawnSharkEvent() {
    sharkEvent.active    = true;
    sharkEvent.direction = (rand() % 2 == 0) ? 1.0f : -1.0f;
    sharkEvent.speed     = SHARK_SPEED_MIN + (rand() % 100) / 100.0f * (SHARK_SPEED_MAX - SHARK_SPEED_MIN);
    sharkEvent.y         = SHARK_Y_MIN + rand() % (int)(SHARK_Y_MAX - SHARK_Y_MIN);
    sharkEvent.x         = (sharkEvent.direction > 0.0f) ? -50.0f : 450.0f;
    sharkEvent.swimPhase = (float)(rand() % 360);
}

static void initHourEvent() {
    hourEvent.phase           = HOUR_EVENT_IDLE;
    hourEvent.schoolFishCount = 0;
}

static void repopulateNormalFauna() {

    for (int i = 0; i < FISH_COUNT; i++) {
        if (fishes[i].entryDelayFrames > 0) {
            fishes[i].x                = -999.0f;
            fishes[i].entryDelayFrames = rand() % 601;
        }

    }
    int activeSchoolCount = 1 + (rand() % SCHOOL_COUNT_MAX);
    int reactivated = 0;
    for (int i = 0; i < SCHOOL_COUNT_MAX; i++) {
        if (schools[i].active) continue;

        if (reactivated < activeSchoolCount) {
            schools[i].willActivate     = true;
            schools[i].entryDelayFrames = rand() % 601;
            reactivated++;
        } else {
            schools[i].willActivate     = false;
            schools[i].entryDelayFrames = 0;
        }
    }

}

static void drawSchoolFishAt(float fx, float fy, float sc, bool ltr, float swimPhase, float t);

static void buildHourSchoolFormation(int hourValue, bool onTopScreen) {
    char text[6];
    snprintf(text, sizeof(text), "%02d:00", hourValue);

    float charWidth   = (HOUR_SCHOOL_FONT_COLS - 1) * HOUR_SCHOOL_CELL_SIZE;
    float totalWidth  = charWidth * 5 + HOUR_SCHOOL_DIGIT_GAP * 4;
    float totalHeight = (HOUR_SCHOOL_FONT_ROWS - 1) * HOUR_SCHOOL_CELL_SIZE;

    int count = 0;
    for (int c = 0; c < 5 && count < HOUR_EVENT_MAX_FISH; c++) {
        int charValue = (text[c] == ':') ? HOUR_SCHOOL_COLON_INDEX : (text[c] - '0');
        float charOriginX = c * (charWidth + HOUR_SCHOOL_DIGIT_GAP) - totalWidth * 0.5f;

        for (int row = 0; row < HOUR_SCHOOL_FONT_ROWS && count < HOUR_EVENT_MAX_FISH; row++) {
            const char* rowStr = HOUR_DIGIT_FONT[charValue][row];
            for (int col = 0; col < HOUR_SCHOOL_FONT_COLS && count < HOUR_EVENT_MAX_FISH; col++) {
                if (rowStr[col] != '1') continue;

                HourSchoolFish* sf = &hourEvent.schoolFish[count];
                sf->offsetX = charOriginX + col * HOUR_SCHOOL_CELL_SIZE;
                sf->offsetY = row * HOUR_SCHOOL_CELL_SIZE - totalHeight * 0.5f;
                sf->bobPhase  = (float)(rand() % 360);
                sf->swimPhase = (float)(rand() % 360);
                sf->scale     = 0.65f + (rand() % 100) / 500.0f;
                count++;
            }
        }
    }

    hourEvent.schoolFishCount   = count;
    hourEvent.schoolOnTopScreen = onTopScreen;
    hourEvent.schoolHourValue   = hourValue;
    hourEvent.groupDirection    = -1.0f;
    hourEvent.groupSpeed        = 0.55f + (rand() % 100) / 400.0f;

    if (onTopScreen) {

        float requestedY  = 50.0f;
        float minTopMargin = 10.0f;
        float halfHeight   = totalHeight * 0.5f;
        hourEvent.groupCenterY = (requestedY - halfHeight < minTopMargin)
                                    ? (minTopMargin + halfHeight)
                                    : requestedY;
    } else {
        hourEvent.groupCenterY = 105.0f;

    }

    float screenWidth = onTopScreen ? 400.0f : 320.0f;
    float startMargin = totalWidth * 0.5f + 20.0f;
    hourEvent.groupCenterX = (hourEvent.groupDirection > 0.0f) ? -startMargin : (screenWidth + startMargin);

    hourEvent.phase = HOUR_EVENT_SCHOOL_FORMATION;
}

static void updateAndDrawHourSchoolFormation(float t, bool onTopScreen, bool drawEnabled) {
    if (hourEvent.phase != HOUR_EVENT_SCHOOL_FORMATION) return;
    if (hourEvent.schoolOnTopScreen != onTopScreen) return;

    hourEvent.groupCenterX += hourEvent.groupDirection * hourEvent.groupSpeed;

    bool ltr = (hourEvent.groupDirection > 0.0f);
    if (drawEnabled) {
        for (int i = 0; i < hourEvent.schoolFishCount; i++) {
            HourSchoolFish* sf = &hourEvent.schoolFish[i];
            float fx = hourEvent.groupCenterX + sf->offsetX;
            float fy = hourEvent.groupCenterY + sf->offsetY + sinf(t * 1.1f + sf->bobPhase) * 1.5f;
            drawSchoolFishAt(fx, fy, sf->scale, ltr, sf->swimPhase, t);
        }
    }

    float screenWidth = onTopScreen ? 400.0f : 320.0f;
    bool allOut = ltr ? (hourEvent.groupCenterX - 110.0f > screenWidth)
                       : (hourEvent.groupCenterX + 110.0f < 0.0f);
    if (allOut) {
        if (!onTopScreen) {

            buildHourSchoolFormation(hourEvent.schoolHourValue, true);
        } else {

            hourEvent.phase = HOUR_EVENT_IDLE;
            repopulateNormalFauna();
        }
    }
}

static void initAbyssEvent() {
    abyssEvent.phase           = ABYSS_WAITING;
    abyssEvent.waitFramesLeft  = ABYSS_AVG_WAIT_FRAMES +
        (rand() % (2 * ABYSS_WAIT_VARIANCE + 1)) - ABYSS_WAIT_VARIANCE;
}

static void spawnAbyssEvent() {
    abyssEvent.phase     = ABYSS_ACTIVE;
    abyssEvent.direction = (rand() % 2 == 0) ? 1.0f : -1.0f;
    abyssEvent.speed     = ABYSS_SPEED_MIN + (rand() % 100) / 100.0f * (ABYSS_SPEED_MAX - ABYSS_SPEED_MIN);
    abyssEvent.y         = ABYSS_Y_MIN + rand() % (int)(ABYSS_Y_MAX - ABYSS_Y_MIN);
    abyssEvent.x         = (abyssEvent.direction > 0.0f) ? -60.0f : 460.0f;

    abyssEvent.bobPhase        = (float)(rand() % 360);
    abyssEvent.pulsePhase      = 0.0f;
    abyssEvent.lightSweepPhase = 0.0f;
    abyssEvent.propellerAngle  = 0.0f;
    abyssEvent.trailCount      = 0;
    abyssEvent.trailDistAccum  = 0.0f;
}

static void initOceanLife() {
    srand(time(NULL));
    initColorPresets();

    for (int i = 0; i < PLANKTON_COUNT; i++) {
        planktons[i].x         = rand() % 400;
        planktons[i].y         = rand() % 240;
        planktons[i].phase     = rand() % 360;
        planktons[i].speed     = 0.2f  + (rand() % 100) / 400.0f;
        planktons[i].base      = 0.3f  + (rand() % 70)  / 100.0f;
        planktons[i].size      = 0.6f  + (rand() % 100) / 200.0f;
        planktons[i].pulseFreq = 0.5f  + (rand() % 100) / 100.0f;
        planktons[i].pulseAmp  = 0.2f  + (rand() % 80)  / 100.0f;
    }

    for (int i = 0; i < BUBBLE_COUNT; i++) {
        initSingleBubble(i, true);
    }
    for (int i = 0; i < BOTTOM_BUBBLE_COUNT; i++) {
        initSingleBottomBubble(i, true);
    }
    bottomJellyCount = 0;

    initSingleJellyfish(0, 130.0f, 90.0f, 1.15f, 0.42f, JELLY_TENTACLE_MAX);

    initSingleJellyfish(1, 290.0f, 150.0f, 0.7f, 0.5f, 4);

    for (int i = 0; i < CAUSTIC_COUNT; i++) {
        float slot = (400.0f / (float)CAUSTIC_COUNT);
        caustics[i].baseX     = slot * (float)i + slot * 0.5f + (float)(rand() % 40 - 20);
        caustics[i].topY      = -10.0f - (rand() % 20);
        caustics[i].length    = 90.0f + (rand() % 70);
        caustics[i].width     = 2.0f + (rand() % 100) / 50.0f;
        caustics[i].swayPhase = (float)(rand() % 360);
        caustics[i].swaySpeed = 0.35f + (rand() % 100) / 200.0f;
        caustics[i].swayAmp   = 3.0f + (rand() % 100) / 20.0f;
        caustics[i].alphaBase = 0.05f + (rand() % 100) / 1400.0f;
    }

    for (int i = 0; i < FISH_COUNT; i++) {
        fishes[i].x = -999.0f;
        fishes[i].entryDelayFrames = rand() % 601;
    }

    int activeSchoolCount = 1 + (rand() % SCHOOL_COUNT_MAX);
    for (int i = 0; i < SCHOOL_COUNT_MAX; i++) {
        schools[i].active = false;
        if (i < activeSchoolCount) {
            schools[i].willActivate     = true;
            schools[i].entryDelayFrames = rand() % 601;
        } else {
            schools[i].willActivate     = false;
            schools[i].entryDelayFrames = 0;
        }
    }

    initAnglerfish();
    initAbyssEvent();
    initTurtleEvent();
    initSharkEvent();
    initHourEvent();
    for (int i = 0; i < JELLY_DRIFTER_MAX_ACTIVE; i++) {
        initJellyDrifterSlot(i);

        jellyDrifters[i].lastDirection      = 0.0f;
        jellyDrifters[i].sameDirectionStreak = 0;
    }

    u32 activeTheme = (settings.bgThemeIndex < 5) ? settings.bgThemeIndex : 0;
    currentTopR = depthThemes[activeTheme].topR;
    currentTopG = depthThemes[activeTheme].topG;
    currentTopB = depthThemes[activeTheme].topB;
    currentBotR = depthThemes[activeTheme].botR;
    currentBotG = depthThemes[activeTheme].botG;
    currentBotB = depthThemes[activeTheme].botB;

    u32 themeCol = depthThemes[activeTheme].textMenuColor;
    currentThemeTextR = (float)(themeCol & 0xFF);
    currentThemeTextG = (float)((themeCol >> 8) & 0xFF);
    currentThemeTextB = (float)((themeCol >> 16) & 0xFF);

    if (settings.clockColorIndex < 15) {
        u32 targetCol = clockPresets[settings.clockColorIndex];
        currentClockR = (float)(targetCol & 0xFF);
        currentClockG = (float)((targetCol >> 8) & 0xFF);
        currentClockB = (float)((targetCol >> 16) & 0xFF);
    }
}

static void updateAndDrawBubbles(float t) {
    u32 activeTheme = (settings.bgThemeIndex < 5) ? settings.bgThemeIndex : 0;
    u32 themeColor  = depthThemes[activeTheme].bubbleColor;

    u8 r = (u8)(themeColor & 0xFF);
    u8 g = (u8)((themeColor >> 8) & 0xFF);
    u8 b = (u8)((themeColor >> 16) & 0xFF);

    for (int i = 0; i < BUBBLE_COUNT; i++) {
        bubbles[i].y -= bubbles[i].speedY;
        bubbles[i].x += sinf(t * bubbles[i].wobbleSpeed + bubbles[i].wobblePhase) * bubbles[i].wobbleAmp * 0.3f;

        float fadeFactor = bubbles[i].y / 40.0f;
        if (fadeFactor > 1.0f) fadeFactor = 1.0f;
        if (fadeFactor < 0.0f) fadeFactor = 0.0f;

        float currentAlpha = bubbles[i].alpha * fadeFactor;

        if (currentAlpha > 0.0f && bubbles[i].y < 245.0f && bubbles[i].x >= 0 && bubbles[i].x <= 400) {
            u8 a = (u8)(currentAlpha * 255);
            u32 fillColor = C2D_Color32(r, g, b, (u8)(a * 0.35f));
            u32 ringColor = C2D_Color32(r, g, b, a);
            C2D_DrawCircleSolid(bubbles[i].x, bubbles[i].y, 0.0f, bubbles[i].size, fillColor);

            C2D_DrawCircleSolid(bubbles[i].x - bubbles[i].size * 0.3f, bubbles[i].y - bubbles[i].size * 0.3f,
                                 0.0f, bubbles[i].size * 0.35f, ringColor);
        }

        if (bubbles[i].y <= -10.0f || currentAlpha <= 0.0f || bubbles[i].x < -10 || bubbles[i].x > 410) {
            initSingleBubble(i, false);
        }
    }
}

static void updateAndDrawBottomBubbles(float t) {
    u8 r = 150, g = 200, b = 230;

    for (int i = 0; i < BOTTOM_BUBBLE_COUNT; i++) {
        bottomBubbles[i].y -= bottomBubbles[i].speedY;
        bottomBubbles[i].x += sinf(t * bottomBubbles[i].wobbleSpeed + bottomBubbles[i].wobblePhase) * bottomBubbles[i].wobbleAmp * 0.3f;

        float fadeFactor = bottomBubbles[i].y / 40.0f;
        if (fadeFactor > 1.0f) fadeFactor = 1.0f;
        if (fadeFactor < 0.0f) fadeFactor = 0.0f;

        float currentAlpha = bottomBubbles[i].alpha * fadeFactor;

        if (currentAlpha > 0.0f && bottomBubbles[i].y < 245.0f && bottomBubbles[i].x >= 0 && bottomBubbles[i].x <= 320) {
            u8 a = (u8)(currentAlpha * 255);
            u32 fillColor = C2D_Color32(r, g, b, (u8)(a * 0.35f));
            u32 ringColor = C2D_Color32(r, g, b, a);
            C2D_DrawCircleSolid(bottomBubbles[i].x, bottomBubbles[i].y, 0.0f, bottomBubbles[i].size, fillColor);
            C2D_DrawCircleSolid(bottomBubbles[i].x - bottomBubbles[i].size * 0.3f, bottomBubbles[i].y - bottomBubbles[i].size * 0.3f,
                                 0.0f, bottomBubbles[i].size * 0.35f, ringColor);
        }

        if (bottomBubbles[i].y <= -10.0f || currentAlpha <= 0.0f || bottomBubbles[i].x < -10 || bottomBubbles[i].x > 330) {
            initSingleBottomBubble(i, false);
        }
    }
}

static void drawPlanktons(float t) {
    for (int i = 0; i < PLANKTON_COUNT; i++) {
        float wave      = sinf(t * planktons[i].pulseFreq + planktons[i].phase);
        float blink     = 0.5f + 0.5f * wave; blink *= blink;

        float intensity = planktons[i].base * (0.18f + blink * planktons[i].pulseAmp * 0.6f);
        u8    c         = (u8)(6 + intensity * 150);
        float s         = planktons[i].size * (0.45f + blink * 0.7f);
        u8 pr = (u8)(c * 0.55f);
        u8 pg = (u8)(c * 0.92f);
        u8 pb = (u8)(c);

        u8 a  = (u8)(90 + blink * 70);
        C2D_DrawRectSolid(planktons[i].x, planktons[i].y, 0.0f, s, s, C2D_Color32(pr, pg, pb, a));
    }
}

static void updateAndDrawJellyfish(int idx, float t) {
    Jellyfish* j = &jellies[idx];

    j->x = j->baseX + sinf(t * j->driftSpeedX + j->driftPhaseX) * j->driftAmpX;
    j->y = j->baseY + sinf(t * j->driftSpeedY + j->driftPhaseY) * j->driftAmpY;

    float pulse     = sinf(t * j->pulseSpeed + j->pulsePhase);

    float bellScaleY = (pulse > 0.0f) ? (1.0f - pulse * 0.34f) : (1.0f - pulse * 0.20f);
    float bellScaleX = (pulse > 0.0f) ? (1.0f + pulse * 0.18f) : (1.0f + pulse * 0.12f);
    float glowAlpha   = 0.65f + 0.30f * (0.5f + 0.5f * pulse);

    float sc = j->scale;
    float bx = j->x, by = j->y;

    float bw = 26.0f * sc * bellScaleX;
    float bh = 22.0f * sc * bellScaleY;

    u32 bellColor   = C2D_Color32(14, 28, 48, 235);
    u32 bellEdge    = C2D_Color32(28, 58, 82, 180);
    u32 glowColorIn = C2D_Color32(186, 242, 255, (u8)(glowAlpha * 230));
    u32 glowColorMid= C2D_Color32(95,  224, 232, (u8)(glowAlpha * 120));
    u32 glowColorOut= C2D_Color32(95,  224, 232, (u8)(glowAlpha * 40));

    int tc = j->tentacleCount;

    float tentacleLenMul = (tc >= 8) ? 1.8f : 1.0f;
    for (int k = 0; k < tc; k++) {

        float spread = (tc > 1) ? ((float)k / (float)(tc - 1) - 0.5f) : 0.0f;
        float anchorX = bx + spread * 30.0f * sc * bellScaleX;

        float lobeRadiusAtSpread = bw * 0.30f * (1.0f - fabsf(spread) * 0.3f);

        float anchorY = by + bh * 0.55f + lobeRadiusAtSpread * 0.7f - 3.0f * sc;

        const int SEGMENTS = 5;
        float prevX = anchorX, prevY = anchorY;
        float swayDir = (spread < 0.0f) ? -1.0f : 1.0f;
        if (k == tc / 2 && tc % 2 == 1) swayDir = 0.3f;

        u8 ta = (u8)(140.0f * (1.0f - (float)k * 0.01f));
        u32 tentColor = C2D_Color32(40, 70, 95, ta);

        for (int s = 1; s <= SEGMENTS; s++) {
            float segT     = (float)s / (float)SEGMENTS;
            float segLen   = 34.0f * sc * tentacleLenMul * j->tentacleLenVar[k];
            float waveSway = sinf(t * 1.6f + j->tentaclePhase[k] + segT * 2.4f) * (4.0f * sc) * segT;
            float curX = anchorX + waveSway * swayDir + spread * 6.0f * sc * segT;
            float curY = anchorY + segLen * segT;

            C2D_DrawLine(prevX, prevY, tentColor, curX, curY, tentColor, 2.0f * sc, 0.0f);
            prevX = curX; prevY = curY;
        }
    }

    C2D_DrawCircleSolid(bx, by - bh * 0.15f, 0.0f, bw, bellColor);

    int lobeCount = 5;
    for (int l = 0; l < lobeCount; l++) {
        float lf = (float)l / (float)(lobeCount - 1) - 0.5f;
        float lx = bx + lf * bw * 1.5f;
        float ly = by + bh * 0.55f - fabsf(lf) * bh * 0.35f;
        C2D_DrawCircleSolid(lx, ly, 0.0f, bw * 0.30f * (1.0f - fabsf(lf) * 0.3f), bellColor);
    }

    C2D_DrawCircleSolid(bx, by - bh * 0.45f, 0.0f, bw * 0.85f, bellEdge);
    C2D_DrawCircleSolid(bx, by - bh * 0.55f, 0.0f, bw * 0.55f, bellColor);

    float lanternX = bx - bw * 0.25f;
    float lanternY = by;
    C2D_DrawCircleSolid(lanternX, lanternY, 0.0f, 7.0f * sc, glowColorOut);
    C2D_DrawCircleSolid(lanternX, lanternY, 0.0f, 4.2f * sc, glowColorMid);
    C2D_DrawCircleSolid(lanternX, lanternY, 0.0f, 2.0f * sc, glowColorIn);

    if (tc >= 8) {
        float lanternX2 = bx + bw * 0.30f;
        float lanternY2 = by + bh * 0.05f;
        C2D_DrawCircleSolid(lanternX2, lanternY2, 0.0f, 6.0f * sc, glowColorOut);
        C2D_DrawCircleSolid(lanternX2, lanternY2, 0.0f, 3.6f * sc, glowColorMid);
        C2D_DrawCircleSolid(lanternX2, lanternY2, 0.0f, 1.7f * sc, glowColorIn);
    }
}

static void drawJellyfishes(float t) {
    for (int i = 0; i < JELLY_COUNT; i++) {
        updateAndDrawJellyfish(i, t);
    }
}

static void rotateAroundCenter(float px, float py, float cx, float cy, float angleRad, float* outX, float* outY) {
    float dx = px - cx, dy = py - cy;
    float cosA = cosf(angleRad), sinA = sinf(angleRad);
    *outX = cx + dx * cosA - dy * sinA;
    *outY = cy + dx * sinA + dy * cosA;
}

static void drawBottomJellyBody(BottomJelly* j, float t, float sc, float bellScaleX, float bellScaleY, float glowAlpha, float tiltRad, float depth);

static bool updateAndDrawSingleBottomJelly(int idx, float t, float depth, bool drawEnabled) {
    BottomJelly* j = &bottomJellies[idx];

    float pulse = sinf(t * j->pulseSpeed + j->pulsePhase);

    if (j->isLaunching) {

        j->targetDirX = sinf(t * 0.6f + j->pulsePhase) * 0.15f;
        j->targetDirY = -1.0f;
        float ndx = j->dirX + (j->targetDirX - j->dirX) * 0.08f;
        float ndy = j->dirY + (j->targetDirY - j->dirY) * 0.08f;
        float nlen = sqrtf(ndx*ndx + ndy*ndy);
        if (nlen > 0.0001f) { j->dirX = ndx/nlen; j->dirY = ndy/nlen; }

        float launchSpeed = BOTTOM_JELLY_BASE_SPEED * 2.2f;

        j->x += j->dirX * launchSpeed;
        j->y += j->dirY * launchSpeed;

        if (j->y < -45.0f) {

            if (pendingArrivalCount < PENDING_ARRIVAL_MAX) {
                float xTop = j->x * (400.0f / 320.0f);
                if (xTop < 20.0f) xTop = 20.0f;
                if (xTop > 380.0f) xTop = 380.0f;
                pendingArrivalX[pendingArrivalCount]           = xTop;
                pendingArrivalTiltPhase[pendingArrivalCount]   = j->tiltPhase;
                pendingArrivalTiltSpeed[pendingArrivalCount]   = j->tiltSpeed;
                pendingArrivalScale[pendingArrivalCount]       = j->scale;
                pendingArrivalTemperament[pendingArrivalCount] = j->temperament;
                pendingArrivalCount++;
            }
            return true;
        }

        if (drawEnabled) {
            float scLaunch = j->scale;
            float bellScaleYL = (pulse > 0.0f) ? (1.0f - pulse * 0.30f) : (1.0f - pulse * 0.18f);
            float bellScaleXL = (pulse > 0.0f) ? (1.0f + pulse * 0.15f) : (1.0f + pulse * 0.10f);
            float glowAlphaL  = 0.55f + 0.30f * (0.5f + 0.5f * pulse);
            float tiltMaxRadL = 10.0f * 3.14159265f / 180.0f;
            float tiltRadL = tiltMaxRadL * sinf(t * j->tiltSpeed + j->tiltPhase);
            drawBottomJellyBody(j, t, scLaunch, bellScaleXL, bellScaleYL, glowAlphaL, tiltRadL, depth);
        }
        return false;
    }

    float ndx = j->dirX + (j->targetDirX - j->dirX) * 0.0315f;
    float ndy = j->dirY + (j->targetDirY - j->dirY) * 0.0315f;
    float nlen = sqrtf(ndx*ndx + ndy*ndy);
    if (nlen > 0.0001f) { j->dirX = ndx/nlen; j->dirY = ndy/nlen; }

    float speed = BOTTOM_JELLY_BASE_SPEED * (0.8f + 0.4f * j->temperament);
    j->x += j->dirX * speed;
    j->y += j->dirY * speed;

    const float margin = 18.0f;
    if (!j->hasEnteredScreen) {
        bool justEntered = j->enteredFromRight ? (j->x <= 320.0f - margin) : (j->x >= margin);
        if (justEntered) {
            j->hasEnteredScreen = true;
        }
    }
    bool bounced = false;
    bool skipLeftBounce  = !j->hasEnteredScreen && !j->enteredFromRight;
    bool skipRightBounce = !j->hasEnteredScreen &&  j->enteredFromRight;
    if (!skipLeftBounce  && j->x < margin)          { j->x = margin;          j->targetDirX =  fabsf(j->targetDirX); bounced = true; }
    if (!skipRightBounce && j->x > 320.0f - margin) { j->x = 320.0f - margin; j->targetDirX = -fabsf(j->targetDirX); bounced = true; }
    if (j->y < margin)          { j->y = margin;          j->targetDirY =  fabsf(j->targetDirY); bounced = true; }
    if (j->y > 240.0f - margin) { j->y = 240.0f - margin; j->targetDirY = -fabsf(j->targetDirY); bounced = true; }
    if (bounced) {
        float wobble = ((rand() % 100) / 100.0f - 0.5f) * 0.6f;
        float ca = cosf(wobble), sa = sinf(wobble);
        float wdx = j->targetDirX * ca - j->targetDirY * sa;
        float wdy = j->targetDirX * sa + j->targetDirY * ca;
        float wlen = sqrtf(wdx*wdx + wdy*wdy);
        if (wlen > 0.0001f) { j->targetDirX = wdx/wlen; j->targetDirY = wdy/wlen; }
    }

    float sc = j->scale;
    float bellScaleY = (pulse > 0.0f) ? (1.0f - pulse * 0.30f) : (1.0f - pulse * 0.18f);
    float bellScaleX = (pulse > 0.0f) ? (1.0f + pulse * 0.15f) : (1.0f + pulse * 0.10f);
    float glowAlpha  = 0.55f + 0.30f * (0.5f + 0.5f * pulse);

    float tiltMaxRad = 10.0f * 3.14159265f / 180.0f;
    float tiltRad = tiltMaxRad * sinf(t * j->tiltSpeed + j->tiltPhase);

    if (drawEnabled) {
        drawBottomJellyBody(j, t, sc, bellScaleX, bellScaleY, glowAlpha, tiltRad, depth);
    }
    return false;
}

static void drawBottomJellyBody(BottomJelly* j, float t, float sc, float bellScaleX, float bellScaleY, float glowAlpha, float tiltRad, float depth) {
    float bx = j->x, by = j->y;
    float bw = 16.0f * sc * bellScaleX;
    float bh = 14.0f * sc * bellScaleY;

    u32 bellColor    = C2D_Color32(150, 165, 175, 215);
    u32 bellEdge     = C2D_Color32(180, 195, 205, 170);
    u32 glowColorIn  = C2D_Color32(255, 255, 250, (u8)(glowAlpha * 220));
    u32 glowColorMid = C2D_Color32(235, 240, 245, (u8)(glowAlpha * 110));
    u32 tentColor    = C2D_Color32(140, 155, 165, 150);

    int tc = BOTTOM_JELLY_TENTACLES;
    for (int k = 0; k < tc; k++) {
        float spread = (tc > 1) ? ((float)k / (float)(tc - 1) - 0.5f) : 0.0f;
        float localAnchorX = spread * 18.0f * sc * bellScaleX;
        float localAnchorY = bh * 0.55f;

        float anchorX, anchorY;
        rotateAroundCenter(bx + localAnchorX, by + localAnchorY, bx, by, tiltRad, &anchorX, &anchorY);

        const int SEGMENTS = 4;
        float prevX = anchorX, prevY = anchorY;
        float swayDir = (spread < 0.0f) ? -1.0f : 1.0f;

        for (int s = 1; s <= SEGMENTS; s++) {
            float segT = (float)s / (float)SEGMENTS;

            float segLen = 48.0f * sc * j->tentacleLenVar[k];
            float waveSway = sinf(t * 1.6f + j->tentaclePhase[k] + segT * 2.4f) * (7.0f * sc) * segT;

            float localX = localAnchorX + waveSway * swayDir + spread * 4.0f * sc * segT;
            float localY = localAnchorY + segLen * segT;
            float curX, curY;
            rotateAroundCenter(bx + localX, by + localY, bx, by, tiltRad, &curX, &curY);

            C2D_DrawLine(prevX, prevY, tentColor, curX, curY, tentColor, 1.4f * sc, depth);
            prevX = curX; prevY = curY;
        }
    }

    float bodyX, bodyY;
    rotateAroundCenter(bx, by - bh * 0.15f, bx, by, tiltRad, &bodyX, &bodyY);
    C2D_DrawCircleSolid(bodyX, bodyY, depth, bw, bellColor);

    int lobeCount = 3;
    for (int l = 0; l < lobeCount; l++) {
        float lf = (float)l / (float)(lobeCount - 1) - 0.5f;
        float localLx = lf * bw * 1.4f;
        float localLy = bh * 0.5f - fabsf(lf) * bh * 0.3f;
        float lobeX, lobeY;
        rotateAroundCenter(bx + localLx, by + localLy, bx, by, tiltRad, &lobeX, &lobeY);
        C2D_DrawCircleSolid(lobeX, lobeY, depth, bw * 0.28f * (1.0f - fabsf(lf) * 0.3f), bellColor);
    }

    float edgeX, edgeY;
    rotateAroundCenter(bx, by - bh * 0.45f, bx, by, tiltRad, &edgeX, &edgeY);
    C2D_DrawCircleSolid(edgeX, edgeY, depth, bw * 0.8f, bellEdge);

    float localLanternX = -bw * 0.2f;
    float lanternX, lanternY;
    rotateAroundCenter(bx + localLanternX, by, bx, by, tiltRad, &lanternX, &lanternY);
    C2D_DrawCircleSolid(lanternX, lanternY, depth, 4.0f * sc, glowColorMid);
    C2D_DrawCircleSolid(lanternX, lanternY, depth, 1.8f * sc, glowColorIn);
}

static void updateAndDrawBottomJellies(float t, float depth, bool drawEnabled) {
    int i = 0;
    while (i < bottomJellyCount) {
        bool shouldRemove = updateAndDrawSingleBottomJelly(i, t, depth, drawEnabled);
        if (shouldRemove) {
            for (int k = i; k < bottomJellyCount - 1; k++) {
                bottomJellies[k] = bottomJellies[k + 1];
            }
            bottomJellyCount--;

        } else {
            i++;
        }
    }
}

static void updateAndDrawJellyDrifters(float t) {
    for (int idx = 0; idx < JELLY_DRIFTER_MAX_ACTIVE; idx++) {
        JellyDrifter* d = &jellyDrifters[idx];

        if (!d->active) {
            continue;
        }

        float pulse = sinf(t * d->pulseSpeed + d->pulsePhase);
        bool isContracting = (pulse > 0.0f);

        if (d->arrivalBurstLeft > 0) {
            if (d->arrivalBurstCooldown > 0) {
                d->arrivalBurstCooldown--;
            } else {
                d->thrustBoost = 2.6f;
                d->arrivalBurstLeft--;
                d->arrivalBurstCooldown = 14;

                spawnScatterBubbles(d->x - d->direction * 12.0f * d->scale, d->y + 4.0f * d->scale, 2);
            }
        } else if (isContracting && !d->wasContracting) {

            d->thrustBoost = 2.0f + 1.2f * d->temperament;

            spawnScatterBubbles(d->x - d->direction * 12.0f * d->scale, d->y + 4.0f * d->scale, 2);
        }
        d->wasContracting = isContracting;
        float thrustFloor = 0.35f;
        d->thrustBoost = thrustFloor + (d->thrustBoost - thrustFloor) * 0.92f;

        if (d->hesitationFramesLeft > 0) {
            d->hesitationFramesLeft--;
        } else {
            d->hesitationCooldown--;
            if (d->hesitationCooldown <= 0) {

                d->hesitationFramesLeft = (int)(24 + 72 * (1.0f - d->temperament)) + rand() % 21;

                d->hesitationCooldown   = (int)(400 + 2600 * d->temperament) + rand() % 301;
            }
        }
        float hesitationTarget = (d->hesitationFramesLeft > 0) ? 0.05f : 1.0f;
        d->hesitationFactor += (hesitationTarget - d->hesitationFactor) * 0.05f;

        d->x += d->speedX * d->direction * d->thrustBoost * d->hesitationFactor;

        if (d->y > 210.0f) {
            d->y -= 0.4f * d->thrustBoost;
        } else {

            d->y -= d->speedY * d->thrustBoost * d->hesitationFactor;
        }

        bool ltr = (d->direction > 0.0f);
        bool offscreen = ltr ? (d->x > 430.0f) : (d->x < -30.0f);
        if (offscreen) {

            if (hourEvent.phase != HOUR_EVENT_IDLE) {
                d->active         = false;
                d->waitFramesLeft = HOUR_EVENT_PAUSE_DELAY;
            } else {
                initJellyDrifterSlot(idx);
            }
            continue;
        }

        if (d->y < -20.0f) {
            if (hourEvent.phase != HOUR_EVENT_IDLE) {
                d->active         = false;
                d->waitFramesLeft = HOUR_EVENT_PAUSE_DELAY;
            } else {
                initJellyDrifterSlot(idx);
            }
            continue;
        }

        float tiltMaxRad = 10.0f * 3.14159265f / 180.0f;
        float tiltRad = tiltMaxRad * sinf(t * d->tiltSpeed + d->tiltPhase);

        float bellScaleY = (pulse > 0.0f) ? (1.0f - pulse * 0.30f) : (1.0f - pulse * 0.18f);
        float bellScaleX = (pulse > 0.0f) ? (1.0f + pulse * 0.15f) : (1.0f + pulse * 0.10f);
        float glowAlpha = 0.55f + 0.30f * (0.5f + 0.5f * pulse);

        float sc = d->scale;

        float bx = d->x;
        float by = d->y + sinf(t * 0.8f + d->pulsePhase) * 3.0f;

        float bw = 16.0f * sc * bellScaleX;
        float bh = 14.0f * sc * bellScaleY;

        u32 bellColor = C2D_Color32(150, 165, 175, 215);
        u32 bellEdge  = C2D_Color32(180, 195, 205, 170);
        u32 glowColorIn  = C2D_Color32(255, 255, 250, (u8)(glowAlpha * 220));
        u32 glowColorMid = C2D_Color32(235, 240, 245, (u8)(glowAlpha * 110));

        int tc = d->tentacleCount;
        for (int k = 0; k < tc; k++) {
            float spread = (tc > 1) ? ((float)k / (float)(tc - 1) - 0.5f) : 0.0f;
            float localAnchorX = spread * 18.0f * sc * bellScaleX;
            float localAnchorY = bh * 0.55f;

            float anchorX, anchorY;
            rotateAroundCenter(bx + localAnchorX, by + localAnchorY, bx, by, tiltRad, &anchorX, &anchorY);

            const int SEGMENTS = 4;
            float prevX = anchorX, prevY = anchorY;
            float swayDir = (spread < 0.0f) ? -1.0f : 1.0f;

            u32 tentColor = C2D_Color32(140, 155, 165, 150);

            for (int s = 1; s <= SEGMENTS; s++) {
                float segT = (float)s / (float)SEGMENTS;
                float segLen = 48.0f * sc * d->tentacleLenVar[k];
                float waveSway = sinf(t * 1.6f + d->tentaclePhase[k] + segT * 2.4f) * (7.0f * sc) * segT;

                float localX = localAnchorX + waveSway * swayDir + spread * 4.0f * sc * segT;
                float localY = localAnchorY + segLen * segT;
                float curX, curY;
                rotateAroundCenter(bx + localX, by + localY, bx, by, tiltRad, &curX, &curY);

                C2D_DrawLine(prevX, prevY, tentColor, curX, curY, tentColor, 1.4f * sc, 0.0f);
                prevX = curX; prevY = curY;
            }
        }

        float bodyX, bodyY;
        rotateAroundCenter(bx, by - bh * 0.15f, bx, by, tiltRad, &bodyX, &bodyY);
        C2D_DrawCircleSolid(bodyX, bodyY, 0.0f, bw, bellColor);

        int lobeCount = 3;
        for (int l = 0; l < lobeCount; l++) {
            float lf = (float)l / (float)(lobeCount - 1) - 0.5f;
            float localLx = lf * bw * 1.4f;
            float localLy = bh * 0.5f - fabsf(lf) * bh * 0.3f;
            float lobeX, lobeY;
            rotateAroundCenter(bx + localLx, by + localLy, bx, by, tiltRad, &lobeX, &lobeY);
            C2D_DrawCircleSolid(lobeX, lobeY, 0.0f, bw * 0.28f * (1.0f - fabsf(lf) * 0.3f), bellColor);
        }

        float edgeX, edgeY;
        rotateAroundCenter(bx, by - bh * 0.45f, bx, by, tiltRad, &edgeX, &edgeY);
        C2D_DrawCircleSolid(edgeX, edgeY, 0.0f, bw * 0.8f, bellEdge);

        float localLanternX = -bw * 0.2f;
        float localLanternY = 0.0f;
        float lanternX, lanternY;
        rotateAroundCenter(bx + localLanternX, by + localLanternY, bx, by, tiltRad, &lanternX, &lanternY);
        C2D_DrawCircleSolid(lanternX, lanternY, 0.0f, 4.0f * sc, glowColorMid);
        C2D_DrawCircleSolid(lanternX, lanternY, 0.0f, 1.8f * sc, glowColorIn);
    }
}

static void updateFish(int idx, float t) {
    Fish* f = &fishes[idx];

    if (f->entryDelayFrames > 0) {
        f->entryDelayFrames--;
        if (f->entryDelayFrames > 0) {
            return;
        }
        initSingleFish(idx, false);
        return;
    }

    f->x += f->speed * f->direction;
    f->y  = f->baseY + sinf(t * f->bobSpeed + f->bobPhase) * f->bobAmp;

    bool ltr = (f->direction > 0.0f);
    bool offscreen = ltr ? (f->x > 430.0f) : (f->x < -30.0f);
    if (offscreen) {

        if (hourEvent.phase != HOUR_EVENT_IDLE) {
            f->entryDelayFrames = HOUR_EVENT_PAUSE_DELAY;
        } else {
            initSingleFish(idx, false);
        }
    }
}

static void drawFish(int idx, float t) {
    Fish* f = &fishes[idx];

    if (f->entryDelayFrames > 0) return;

    bool ltr = (f->direction > 0.0f);
    float sc = f->scale;
    float fx = f->x, fy = f->y;

    float tailWag = sinf(t * 4.0f + f->swimPhase) * 3.0f * sc;

    float finFlap = sinf(t * 4.0f + f->swimPhase + 1.6f) * 0.5f + 0.5f;

    u32 bodyColor = C2D_Color32(20, 45, 65, 235);
    u32 finColor  = C2D_Color32(28, 56, 80, 200);
    u32 finColorLt= C2D_Color32(38, 72, 100, 150);
    u32 eyeGlow   = C2D_Color32(150, 235, 245, 220);

    float bodyR = 7.0f * sc;

    float dirX = ltr ? 1.0f : -1.0f;

    C2D_DrawCircleSolid(fx, fy, 0.0f, bodyR, bodyColor);

    float tailRootX = fx - dirX * bodyR * 0.75f;
    float tailFarX  = fx - dirX * bodyR * 2.3f;
    C2D_DrawTriangle(tailRootX, fy,                  finColor,
                      tailFarX,  fy - 2.0f*sc + tailWag, finColor,
                      tailFarX,  fy - 7.0f*sc + tailWag, finColor, 0.0f);
    C2D_DrawTriangle(tailRootX, fy,                  finColor,
                      tailFarX,  fy + 2.0f*sc + tailWag, finColor,
                      tailFarX,  fy + 7.0f*sc + tailWag, finColor, 0.0f);

    float finRootX = fx - dirX * 0.5f * sc;
    float finTipX  = fx - dirX * 2.0f * sc;
    C2D_DrawTriangle(finRootX, fy - bodyR*0.6f,  finColor,
                      finTipX,  fy - bodyR*1.5f,  finColor,
                      finTipX,  fy - bodyR*2.5f,  finColor, 0.0f);
    C2D_DrawTriangle(finRootX, fy + bodyR*0.6f,  finColor,
                      finTipX,  fy + bodyR*1.5f,  finColor,
                      finTipX,  fy + bodyR*2.5f,  finColor, 0.0f);

    float pecRootX = fx + dirX * bodyR * 0.15f;
    float pecSpread = bodyR * (0.7f + finFlap * 0.4f);
    C2D_DrawTriangle(pecRootX, fy + bodyR*0.3f,                    finColorLt,
                      pecRootX - dirX*2.0f*sc, fy + pecSpread,        finColorLt,
                      pecRootX - dirX*3.5f*sc, fy + pecSpread*1.4f,   finColorLt, 0.0f);

    float eyeX = fx + dirX * bodyR * 0.55f;
    C2D_DrawCircleSolid(eyeX, fy - bodyR * 0.15f, 0.0f, 1.6f * sc, eyeGlow);
    C2D_DrawCircleSolid(eyeX, fy - bodyR * 0.15f, 0.0f, 0.6f * sc, C2D_Color32(255,255,255,255));
}

#define FISH_SMALL_SCALE_THRESHOLD 1.25f

static void updateFishes(float t) {
    for (int i = 0; i < FISH_COUNT; i++) {
        updateFish(i, t);
    }
}

static void drawFishesBehindJellies(float t) {
    for (int i = 0; i < FISH_COUNT; i++) {
        if (fishes[i].scale < FISH_SMALL_SCALE_THRESHOLD) {
            drawFish(i, t);
        }
    }
}

static void drawFishesInFrontOfJellies(float t) {
    for (int i = 0; i < FISH_COUNT; i++) {
        if (fishes[i].scale >= FISH_SMALL_SCALE_THRESHOLD) {
            drawFish(i, t);
        }
    }
}

static void updateSchools(float t) {
    (void)t;
    for (int i = 0; i < SCHOOL_COUNT_MAX; i++) {
        School* s = &schools[i];

        if (!s->active) {
            if (s->willActivate && s->entryDelayFrames > 0) {
                s->entryDelayFrames--;
                if (s->entryDelayFrames == 0) {
                    initSingleSchool(i, false);
                }
            }
            continue;
        }

        s->centerX += s->speed * s->direction;

        bool ltr = (s->direction > 0.0f);

        bool offscreen = ltr ? (s->centerX > 460.0f) : (s->centerX < -60.0f);
        if (offscreen) {

            if (hourEvent.phase != HOUR_EVENT_IDLE) {
                s->active           = false;
                s->willActivate     = false;
                s->entryDelayFrames = HOUR_EVENT_PAUSE_DELAY;
            } else {
                initSingleSchool(i, false);
            }
        }
    }
}

static void drawSchoolFishAt(float fx, float fy, float sc, bool ltr, float swimPhase, float t) {
    float tailWag = sinf(t * 4.5f + swimPhase) * 2.0f * sc;
    float dirX = ltr ? 1.0f : -1.0f;
    float bodyR = 4.0f * sc;

    u32 bodyColor = C2D_Color32(25, 50, 70, 200);
    u32 finColor  = C2D_Color32(32, 62, 86, 170);

    float tailRootX = fx - dirX * bodyR * 0.7f;
    float tailFarX  = fx - dirX * bodyR * 2.0f;
    C2D_DrawTriangle(tailRootX, fy,                      finColor,
                      tailFarX,  fy - 2.5f*sc + tailWag,    finColor,
                      tailFarX,  fy + 2.5f*sc + tailWag,    finColor, 0.0f);

    C2D_DrawCircleSolid(fx, fy, 0.0f, bodyR, bodyColor);
}

static void updateAndDrawSchools(float t) {
    for (int i = 0; i < SCHOOL_COUNT_MAX; i++) {
        School* s = &schools[i];
        if (!s->active) continue;

        bool ltr = (s->direction > 0.0f);

        for (int k = 0; k < s->fishCount; k++) {
            SchoolFish* sf = &schoolFish[s->fishStartIdx + k];
            float fx = s->centerX + sf->offsetX;
            float fy = s->centerY + sf->offsetY + sinf(t * 1.1f + sf->bobPhase) * sf->bobAmp;
            drawSchoolFishAt(fx, fy, sf->scale, ltr, sf->swimPhase, t);
        }
    }
}

static void updateAndDrawAnglerfish(float t) {
    if (!anglerfish.active) {

        return;
    }

    anglerfish.x += anglerfish.speed * anglerfish.direction;

    bool ltr = (anglerfish.direction > 0.0f);
    bool offscreen = ltr ? (anglerfish.x > 440.0f) : (anglerfish.x < -40.0f);
    if (offscreen) {
        anglerfish.active = false;
        return;
    }

    float dirX = ltr ? 1.0f : -1.0f;
    float fx = anglerfish.x, fy = anglerfish.y;
    float bodyWobble = sinf(t * 1.4f + anglerfish.swimPhase) * 1.5f;

    u32 bodyColor = C2D_Color32(8, 12, 18, 235);
    u32 finColor  = C2D_Color32(6, 9, 14, 200);
    u32 eyeGlow   = C2D_Color32(150, 235, 245, 220);

    float bodyR = 11.0f;
    C2D_DrawCircleSolid(fx, fy + bodyWobble, 0.6f, bodyR, bodyColor);
    C2D_DrawCircleSolid(fx - dirX * bodyR * 0.9f, fy + bodyWobble, 0.6f, bodyR * 0.55f, bodyColor);

    float tailRootX = fx - dirX * bodyR * 1.5f;
    float tailFarX  = fx - dirX * bodyR * 2.6f;
    C2D_DrawTriangle(tailRootX, fy + bodyWobble,                    finColor,
                      tailFarX,  fy + bodyWobble - 5.0f,             finColor,
                      tailFarX,  fy + bodyWobble + 5.0f,             finColor, 0.6f);

    float baseIllX = fx + dirX * bodyR * 0.3f;
    float baseIllY = fy + bodyWobble - bodyR * 0.7f;
    float midIllX  = baseIllX + dirX * 3.0f;
    float midIllY  = baseIllY - 8.0f;
    float tipIllX  = baseIllX + dirX * 7.0f;
    float tipIllY  = baseIllY - 14.0f;
    C2D_DrawLine(baseIllX, baseIllY, finColor, midIllX, midIllY, finColor, 1.6f, 0.6f);
    C2D_DrawLine(midIllX,  midIllY,  finColor, tipIllX, tipIllY, finColor, 1.4f, 0.6f);

    float eyeX = fx + dirX * bodyR * 0.55f;
    float eyeY = fy + bodyWobble - bodyR * 0.15f;
    C2D_DrawCircleSolid(eyeX, eyeY, 0.6f, 1.6f, eyeGlow);
    C2D_DrawCircleSolid(eyeX, eyeY, 0.6f, 0.6f, C2D_Color32(255,255,255,255));

    float pulse = sinf(t * 1.8f + anglerfish.lanternPhase);
    float glowAlpha = 0.7f + 0.3f * (0.5f + 0.5f * pulse);
    u32 glowOut = C2D_Color32(186, 242, 255, (u8)(glowAlpha * 70));
    u32 glowMid = C2D_Color32(150, 235, 245, (u8)(glowAlpha * 140));
    u32 glowIn  = C2D_Color32(220, 250, 255, (u8)(glowAlpha * 230));
    C2D_DrawCircleSolid(tipIllX, tipIllY, 0.65f, 6.5f, glowOut);
    C2D_DrawCircleSolid(tipIllX, tipIllY, 0.65f, 3.8f, glowMid);
    C2D_DrawCircleSolid(tipIllX, tipIllY, 0.65f, 1.7f, glowIn);
}

static void drawBabyTurtle(float cx, float cy, float dirX, float scale, float finAngleDeg) {

    u32 shellColor = C2D_Color32(26, 58, 44, 160);
    u32 finColor   = C2D_Color32(115, 128, 138, 156);
    u32 eyeColor   = C2D_Color32(58, 74, 85, 173);

    float shellHalfW = 8.0f * scale, shellHalfH = 2.6f * scale;
    C2D_DrawRectSolid(cx - shellHalfW, cy - shellHalfH, 0.0f, shellHalfW * 2.0f, shellHalfH * 2.0f, shellColor);
    C2D_DrawCircleSolid(cx - dirX * shellHalfW, cy, 0.0f, shellHalfH, shellColor);
    C2D_DrawCircleSolid(cx + dirX * shellHalfW, cy, 0.0f, shellHalfH, shellColor);

    float headR = 1.6f * scale;
    float headCx = cx + dirX * (shellHalfW + headR + 1.2f * scale);
    C2D_DrawCircleSolid(headCx, cy, 0.0f, headR, shellColor);
    C2D_DrawCircleSolid(headCx + dirX * headR * 0.5f, cy - headR * 0.3f, 0.0f, 0.45f * scale, eyeColor);

    float tailBaseX = cx - dirX * shellHalfW;
    float tailTipX  = tailBaseX - dirX * 5.0f * scale;
    C2D_DrawTriangle(tailBaseX, cy - shellHalfH * 0.5f, shellColor,
                      tailBaseX, cy + shellHalfH * 0.5f, shellColor,
                      tailTipX,  cy,                       shellColor, 0.0f);

    float finAngleRad  = finAngleDeg * 3.14159265f / 180.0f;
    float restAngleRad = 45.0f * 3.14159265f / 180.0f * dirX;
    float finTotalAngle = restAngleRad + finAngleRad * dirX;

    float finAnchorFrontX = cx + dirX * (shellHalfW * 0.7f);
    float finAnchorBackX  = cx - dirX * (shellHalfW * 0.7f);
    float finAnchorY = cy + shellHalfH * 0.4f;

    for (int f = 0; f < 2; f++) {
        float anchorX = (f == 0) ? finAnchorFrontX : finAnchorBackX;
        float localTipX = 0.0f,         localTipY = 14.0f * scale;
        float localMidX = -3.0f * scale, localMidY = 7.0f * scale;
        float tipX, tipY, midX, midY;
        rotateAroundCenter(anchorX + localTipX, finAnchorY + localTipY, anchorX, finAnchorY, finTotalAngle, &tipX, &tipY);
        rotateAroundCenter(anchorX + localMidX, finAnchorY + localMidY, anchorX, finAnchorY, finTotalAngle, &midX, &midY);

        C2D_DrawTriangle(anchorX, finAnchorY, finColor,
                          midX, midY, finColor,
                          tipX, tipY, finColor, 0.0f);
    }
}

static void updateAndDrawTurtleEvent(float t) {
    if (!turtleEvent.active) {

        if (turtleEvent.waitFramesLeft > 0) {
            turtleEvent.waitFramesLeft--;
        } else {

            time_t rawNow = time(NULL);
            struct tm* tmvNow = localtime(&rawNow);
            if (tmvNow->tm_min == 0) {
                turtleEvent.waitFramesLeft = 300 + rand() % 300;
            } else {
                spawnTurtleEvent();
            }
        }
        return;
    }

    turtleEvent.strokeFrame = (turtleEvent.strokeFrame + 1) % TURTLE_STROKE_CYCLE_FRAMES;
    float cycleT = (float)turtleEvent.strokeFrame / (float)TURTLE_STROKE_CYCLE_FRAMES;

    float finAngleDeg;
    float targetBoost;

    const float halfCycle = 0.5f;
    if (cycleT < halfCycle) {

        float u = cycleT / halfCycle;
        finAngleDeg  = 30.0f * u;
        targetBoost  = 1.0f + 2.0f * u;
    } else {

        float u = (cycleT - halfCycle) / (1.0f - halfCycle);
        finAngleDeg  = 30.0f * (1.0f - u);
        targetBoost  = 3.0f - 2.3f * u;
    }

    turtleEvent.speedBoost += (targetBoost - turtleEvent.speedBoost) * 0.15f;

    const float TURTLE_BASE_SPEED = 0.115f;
    turtleEvent.x += TURTLE_BASE_SPEED * turtleEvent.speedBoost * turtleEvent.direction;

    bool ltr = (turtleEvent.direction > 0.0f);

    bool offscreen = ltr ? (turtleEvent.x > 500.0f) : (turtleEvent.x < -100.0f);
    if (offscreen) {
        turtleEvent.active = false;
        initTurtleEvent();
        return;
    }

    float dirX = ltr ? 1.0f : -1.0f;

    float tx = turtleEvent.x;
    float ty = turtleEvent.y + sinf(t * 1.1f + turtleEvent.bobPhase) * 2.5f;

    u32 shellColor = C2D_Color32(26, 58, 44, 160);
    u32 shellEdge  = C2D_Color32(44, 90, 68, 136);
    u32 plastronColor = C2D_Color32(200, 195, 160, 156);
    u32 eyeGlow    = C2D_Color32(150, 235, 245, 150);
    u32 finColor   = C2D_Color32(115, 128, 138, 156);

    float headHalfW = 5.0f, headHalfH = 3.2f;

    float shellHalfW = 17.0f, shellHalfH = 5.0f;
    float shellCx = tx, shellCy = ty;
    C2D_DrawRectSolid(shellCx - shellHalfW, shellCy - shellHalfH, 0.0f, shellHalfW * 2.0f, shellHalfH * 2.0f, shellColor);
    C2D_DrawCircleSolid(shellCx - dirX * shellHalfW, shellCy, 0.0f, shellHalfH, shellColor);
    C2D_DrawCircleSolid(shellCx + dirX * shellHalfW, shellCy, 0.0f, shellHalfH, shellColor);

    float plastronHalfH = shellHalfH * 0.55f;
    float plastronCy = shellCy + shellHalfH * 0.4f;
    float plastronBackX  = shellCx - dirX * (shellHalfW * 0.82f);
    float plastronFrontX = shellCx + dirX * (shellHalfW + headHalfW + 6.0f);

    float plastronMinX = fminf(plastronBackX, plastronFrontX);
    float plastronMaxX = fmaxf(plastronBackX, plastronFrontX);
    C2D_DrawRectSolid(plastronMinX, plastronCy - plastronHalfH, 0.0f, plastronMaxX - plastronMinX, plastronHalfH * 2.0f, plastronColor);
    C2D_DrawCircleSolid(plastronBackX,  plastronCy, 0.0f, plastronHalfH, plastronColor);
    C2D_DrawCircleSolid(plastronFrontX, plastronCy, 0.0f, plastronHalfH, plastronColor);

    const int DOME_SEGMENTS = 5;
    float domeX[DOME_SEGMENTS], domeR[DOME_SEGMENTS];
    for (int i = -DOME_SEGMENTS/2; i <= DOME_SEGMENTS/2; i++) {
        float off = (float)i / (float)(DOME_SEGMENTS/2);
        int idx = i + DOME_SEGMENTS/2;
        domeX[idx] = shellCx + off * shellHalfW * 0.85f;
        domeR[idx] = shellHalfH * 1.3f * (1.0f - fabsf(off) * 0.35f);
        C2D_DrawCircleSolid(domeX[idx], shellCy - shellHalfH * 0.55f, 0.0f, domeR[idx], shellColor);
    }

    const float domeBaseY = shellCy - shellHalfH * 0.55f;
    for (int i = 0; i < DOME_SEGMENTS - 1; i++) {

        float midX = (domeX[i] + domeX[i+1]) * 0.5f;
        float avgR = (domeR[i] + domeR[i+1]) * 0.5f;

        float leftX  = midX - avgR * 0.35f, leftY  = domeBaseY - avgR * 0.55f;
        float topX   = midX,                  topY   = domeBaseY - avgR * 0.85f;
        float rightX = midX + avgR * 0.35f, rightY = domeBaseY - avgR * 0.55f;
        C2D_DrawLine(leftX, leftY, shellEdge, topX, topY, shellEdge, 1.0f, 0.0f);
        C2D_DrawLine(topX, topY, shellEdge, rightX, rightY, shellEdge, 1.0f, 0.0f);
    }

    float tailBaseX = shellCx - dirX * shellHalfW;
    float tailTipX  = tailBaseX - dirX * 12.0f;
    C2D_DrawTriangle(tailBaseX, shellCy - shellHalfH * 0.5f, shellColor,
                      tailBaseX, shellCy + shellHalfH * 0.5f, shellColor,
                      tailTipX,  shellCy,                       shellColor, 0.0f);

    float headCx = shellCx + dirX * (shellHalfW + headHalfW + 3.0f);
    float headCy = shellCy;
    C2D_DrawRectSolid(headCx - headHalfW, headCy - headHalfH, 0.0f, headHalfW * 2.0f, headHalfH * 2.0f, shellColor);
    C2D_DrawCircleSolid(headCx - dirX * headHalfW, headCy, 0.0f, headHalfH, shellColor);
    C2D_DrawCircleSolid(headCx + dirX * headHalfW, headCy, 0.0f, headHalfH, shellColor);

    float eyeX = headCx + dirX * headHalfW * 0.6f;
    float eyeY = headCy - headHalfH * 0.3f;

    if (turtleEvent.blinkDurationLeft > 0) {
        turtleEvent.blinkDurationLeft--;
        if (turtleEvent.blinkDurationLeft == 0) {
            turtleEvent.blinkFramesLeft = 90 + rand() % 180;
        }
    } else if (turtleEvent.blinkFramesLeft > 0) {
        turtleEvent.blinkFramesLeft--;
        if (turtleEvent.blinkFramesLeft == 0) {
            turtleEvent.blinkDurationLeft = 8;
        }
    }

    bool eyeClosed = (turtleEvent.blinkDurationLeft > 0);
    float eyeHalfW = 1.0f;
    float eyeHalfH = eyeClosed ? 0.25f : 0.7f;

    C2D_DrawLine(eyeX - eyeHalfW, eyeY, eyeGlow, eyeX + eyeHalfW, eyeY, eyeGlow, eyeHalfH * 2.0f, 0.0f);

    float finAngleRad = finAngleDeg * 3.14159265f / 180.0f;

    float restAngleRad = 45.0f * 3.14159265f / 180.0f * dirX;
    float finTotalAngle = restAngleRad + finAngleRad * dirX;

    float finAnchorFrontX = shellCx + dirX * (shellHalfW * 0.7f);
    float finAnchorBackX  = shellCx - dirX * (shellHalfW * 0.7f);
    float finAnchorY = shellCy + shellHalfH * 0.4f;

    for (int f = 0; f < 2; f++) {
        float anchorX = (f == 0) ? finAnchorFrontX : finAnchorBackX;

        float localTipX = 0.0f, localTipY = 14.0f;
        float localMidX = -3.0f, localMidY = 7.0f;
        float tipX, tipY, midX, midY;
        rotateAroundCenter(anchorX + localTipX, finAnchorY + localTipY, anchorX, finAnchorY, finTotalAngle, &tipX, &tipY);
        rotateAroundCenter(anchorX + localMidX, finAnchorY + localMidY, anchorX, finAnchorY, finTotalAngle, &midX, &midY);

        C2D_DrawTriangle(anchorX, finAnchorY, finColor,
                          midX, midY, finColor,
                          tipX, tipY, finColor, 0.0f);
    }

    for (int i = 0; i < TURTLE_BABY_COUNT; i++) {
        TurtleBaby* baby = &turtleBabies[i];

        baby->strokeFrame = (baby->strokeFrame + 1) % TURTLE_BABY_STROKE_CYCLE_FRAMES;
        float babyCycleT = (float)baby->strokeFrame / (float)TURTLE_BABY_STROKE_CYCLE_FRAMES;
        float babyFinAngleDeg;
        if (babyCycleT < 0.5f) {
            babyFinAngleDeg = 30.0f * (babyCycleT / 0.5f);
        } else {
            babyFinAngleDeg = 30.0f * (1.0f - (babyCycleT - 0.5f) / 0.5f);
        }

        float babyCx = shellCx + dirX * baby->offsetX;
        float babyCy = shellCy + baby->offsetY;

        drawBabyTurtle(babyCx, babyCy, dirX, baby->scale, babyFinAngleDeg);
    }
}

static void updateAndDrawSharkEvent(float t) {
    if (!sharkEvent.active) {

        return;
    }

    sharkEvent.x += sharkEvent.speed * sharkEvent.direction;

    bool ltr = (sharkEvent.direction > 0.0f);
    bool offscreen = ltr ? (sharkEvent.x > 450.0f) : (sharkEvent.x < -50.0f);
    if (offscreen) {
        sharkEvent.active = false;
        return;
    }

    float dirX = ltr ? 1.0f : -1.0f;
    float sx = sharkEvent.x, sy = sharkEvent.y;

    float tailWagBase = sinf(t * 3.2f + sharkEvent.swimPhase) * 3.0f;

    u32 bodyColor = C2D_Color32(12, 22, 32, 235);
    u32 finColor  = C2D_Color32(28, 44, 56, 200);
    u32 eyeColor  = C2D_Color32(58, 74, 85, 255);
    u32 eyeGlint  = C2D_Color32(127, 168, 184, 255);

#define SHARK_BODY_POINTS 7

    static const float bodyOffsets[SHARK_BODY_POINTS] = { 21.0f, 14.0f,  6.0f, -1.0f, -8.0f, -14.0f, -19.0f };
    static const float bodyRadii[SHARK_BODY_POINTS]   = {  1.0f,  4.0f,  6.0f,  6.5f,  5.5f,   3.5f,   1.4f };
    float bodyX[SHARK_BODY_POINTS], bodyY[SHARK_BODY_POINTS];
    for (int i = 0; i < SHARK_BODY_POINTS; i++) {
        bodyX[i] = sx + dirX * bodyOffsets[i];
        bodyY[i] = sy;
        C2D_DrawCircleSolid(bodyX[i], bodyY[i], 0.0f, bodyRadii[i], bodyColor);
    }

    float pedX = bodyX[SHARK_BODY_POINTS-1], pedY = bodyY[SHARK_BODY_POINTS-1];

    float upperTipLocalX = -14.0f, upperTipLocalY = -9.0f;
    float notchLocalX    = -7.0f,  notchLocalY    = -2.0f;
    float lowerTipLocalX = -11.0f, lowerTipLocalY  = 3.0f;

    float upperTipX = pedX + dirX * upperTipLocalX, upperTipY = pedY + upperTipLocalY + tailWagBase * 1.0f;
    float notchX     = pedX + dirX * notchLocalX,     notchY     = pedY + notchLocalY     + tailWagBase * 0.5f;
    float lowerTipX  = pedX + dirX * lowerTipLocalX,  lowerTipY  = pedY + lowerTipLocalY  + tailWagBase * 0.7f;

    C2D_DrawTriangle(pedX, pedY - 1.0f, finColor,
                      upperTipX, upperTipY, finColor,
                      notchX, notchY, finColor, 0.0f);
    C2D_DrawTriangle(pedX, pedY + 1.0f, finColor,
                      notchX, notchY, finColor,
                      lowerTipX, lowerTipY, finColor, 0.0f);

    float dorsalBaseFrontX = sx + dirX * 2.0f,  dorsalBaseFrontY = sy - 6.0f;
    float dorsalBaseBackX  = sx + dirX * -6.0f, dorsalBaseBackY  = sy - 5.5f;
    float dorsalApexX      = sx + dirX * -2.0f, dorsalApexY      = sy - 14.5f;
    C2D_DrawTriangle(dorsalBaseFrontX, dorsalBaseFrontY, finColor,
                      dorsalApexX, dorsalApexY, finColor,
                      dorsalBaseBackX, dorsalBaseBackY, finColor, 0.0f);

    float pecRootX = sx + dirX * 6.0f,  pecRootY = sy + 2.0f;
    float pecTip1X = sx + dirX * -2.0f, pecTip1Y = sy + 12.0f;
    float pecTip2X = sx + dirX * -6.0f, pecTip2Y = sy + 15.0f;
    C2D_DrawTriangle(pecRootX, pecRootY, finColor,
                      pecTip1X, pecTip1Y, finColor,
                      pecTip2X, pecTip2Y, finColor, 0.0f);

    for (int g = 0; g < 3; g++) {
        float gx = sx + dirX * (13.0f - g * 2.0f);
        C2D_DrawLine(gx, sy - 5.5f, finColor, gx - dirX * 0.6f, sy + 0.8f, finColor, 0.8f, 0.0f);
    }

    float eyeX = sx + dirX * 16.0f, eyeY = sy - 2.2f;
    C2D_DrawCircleSolid(eyeX, eyeY, 0.0f, 0.9f, eyeColor);
    C2D_DrawCircleSolid(eyeX + dirX * 0.2f, eyeY - 0.2f, 0.0f, 0.35f, eyeGlint);

    float mouthBaseX = sx + dirX * 19.0f, mouthBaseY = sy - 0.5f;
    float mouthMidX  = sx + dirX * 17.0f, mouthMidY  = sy + 0.6f;
    float mouthEndX  = sx + dirX * 15.5f, mouthEndY  = sy + 0.3f;
    C2D_DrawLine(mouthBaseX, mouthBaseY, finColor, mouthMidX, mouthMidY, finColor, 0.7f, 0.0f);
    C2D_DrawLine(mouthMidX, mouthMidY, finColor, mouthEndX, mouthEndY, finColor, 0.7f, 0.0f);
}

static float smoothstep01(float u) {
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;
    return u * u * (3.0f - 2.0f * u);
}

static void updateAndDrawAbyssEvent(float t) {
    if (abyssEvent.phase == ABYSS_WAITING) {

        return;
    }

    abyssEvent.x += abyssEvent.speed * abyssEvent.direction;

    bool ltr = (abyssEvent.direction > 0.0f);
    bool offscreen = ltr ? (abyssEvent.x > 460.0f) : (abyssEvent.x < -60.0f);
    if (offscreen) {
        initAbyssEvent();
        return;
    }

    abyssEvent.pulsePhase      += 0.05f;
    abyssEvent.lightSweepPhase += 0.10f;

    float distFromStart = ltr ? (abyssEvent.x - (-60.0f)) : (460.0f - abyssEvent.x);
    float distFromEnd    = ltr ? (460.0f - abyssEvent.x) : (abyssEvent.x - (-60.0f));
    float fadeIn  = smoothstep01(distFromStart / 30.0f);
    float fadeOut = smoothstep01(distFromEnd / 30.0f);
    float fade = fadeIn * fadeOut;
    if (fade < 0.0f) fade = 0.0f;
    if (fade > 1.0f) fade = 1.0f;

    float ex = abyssEvent.x;
    float ey = abyssEvent.y + sinf(t * 1.4f + abyssEvent.bobPhase) * 2.0f;
    float pulse = 0.5f + 0.5f * sinf(abyssEvent.pulsePhase * 2.2f);

    abyssEvent.propellerAngle += abyssEvent.speed * 0.35f;

    float distanceMix = 0.45f;
    u8 bgMixR = 20, bgMixG = 35, bgMixB = 55;

    #define ABYSS_MIX_COLOR(r,g,b) C2D_Color32( \
        (u8)((r)*(1.0f-distanceMix) + bgMixR*distanceMix), \
        (u8)((g)*(1.0f-distanceMix) + bgMixG*distanceMix), \
        (u8)((b)*(1.0f-distanceMix) + bgMixB*distanceMix), \
        (u8)(255.0f * fade))

    float dirX = (abyssEvent.direction > 0.0f) ? 1.0f : -1.0f;

    float subScale = 0.9f;

    float hullHalfH = 7.5f * subScale;
    float hullBackX  = ex - dirX * 24.0f * subScale;
    float hullFrontX = ex + dirX * 22.0f * subScale;

    u32 hullColor   = ABYSS_MIX_COLOR(58, 74, 88);
    u32 finColor    = ABYSS_MIX_COLOR(48, 64, 78);

    u32 scopeLight  = ABYSS_MIX_COLOR((int)(220 + pulse * 35), (int)(245 + pulse * 10), 255);
    #undef ABYSS_MIX_COLOR

    abyssEvent.trailDistAccum += abyssEvent.speed;
    if (abyssEvent.trailDistAccum >= ABYSS_TRAIL_STEP_PX) {
        abyssEvent.trailDistAccum = 0.0f;
        for (int i = ABYSS_TRAIL_POINTS - 1; i > 0; i--) {
            abyssEvent.trailX[i] = abyssEvent.trailX[i - 1];
            abyssEvent.trailY[i] = abyssEvent.trailY[i - 1];
        }
        abyssEvent.trailX[0] = hullBackX;
        abyssEvent.trailY[0] = ey;
        if (abyssEvent.trailCount < ABYSS_TRAIL_POINTS) abyssEvent.trailCount++;
    }

    for (int i = abyssEvent.trailCount - 1; i >= 0; i--) {
        float ageT = (float)i / (float)(ABYSS_TRAIL_POINTS - 1);
        float bubbleAlpha = (1.0f - ageT) * 130.0f * fade;
        float bubbleR = 1.0f + (1.0f - ageT) * 1.3f;
        u32 bubbleColor = C2D_Color32(150, 190, 210, (u8)bubbleAlpha);
        C2D_DrawCircleSolid(abyssEvent.trailX[i], abyssEvent.trailY[i], 0.0f, bubbleR, bubbleColor);
    }

    C2D_DrawCircleSolid(hullFrontX, ey, 0.0f, hullHalfH, hullColor);

    float hullCenterX = (hullBackX + hullFrontX) * 0.5f;
    float hullHalfW   = fabsf(hullFrontX - hullBackX) * 0.5f;
    C2D_DrawRectSolid(hullCenterX - hullHalfW, ey - hullHalfH, 0.0f, hullHalfW * 2.0f, hullHalfH * 2.0f, hullColor);
    C2D_DrawCircleSolid(hullBackX, ey, 0.0f, hullHalfH, hullColor);

    float towerX = ex - dirX * 4.0f * subScale;
    float towerHalfW = 9.0f * subScale, towerHalfH = 5.5f * subScale;
    float towerTopY = ey - hullHalfH - towerHalfH * 2.0f;
    C2D_DrawRectSolid(towerX - towerHalfW, towerTopY, 0.0f, towerHalfW * 2.0f, towerHalfH * 2.0f, finColor);

    float scopeBaseX = towerX + dirX * towerHalfW * 0.5f;
    float scopeBaseY = towerTopY;
    float scopeTipY  = scopeBaseY - 11.0f * subScale;
    C2D_DrawLine(scopeBaseX, scopeBaseY, finColor, scopeBaseX, scopeTipY, finColor, 1.6f * subScale, 0.0f);
    C2D_DrawCircleSolid(scopeBaseX, scopeTipY - 1.0f * subScale, 0.0f, 1.4f * subScale, scopeLight);

    float finRootX = hullBackX + hullHalfH * 0.3f + dirX * 5.0f * subScale;
    float finTipX  = hullBackX - dirX * 18.0f * subScale + dirX * 5.0f * subScale;

    C2D_DrawTriangle(finRootX, ey - hullHalfH * 0.6f,         finColor,
                      finTipX,  ey - hullHalfH * 2.4f,         finColor,
                      finTipX,  ey - hullHalfH * 0.5f,         finColor, 0.0f);
    C2D_DrawTriangle(finRootX, ey + hullHalfH * 0.6f,         finColor,
                      finTipX,  ey + hullHalfH * 2.4f,         finColor,
                      finTipX,  ey + hullHalfH * 0.5f,         finColor, 0.0f);

    float propX = finTipX + dirX * 3.0f * subScale;
    float bladeLen = 6.5f * subScale;
    for (int b = 0; b < 2; b++) {
        float bladeAngle = abyssEvent.propellerAngle + b * 3.14159265f;
        float bladeApparentLen = bladeLen * fabsf(cosf(bladeAngle));
        float bladeY1 = ey - bladeApparentLen;
        float bladeY2 = ey + bladeApparentLen;
        C2D_DrawLine(propX, bladeY1, finColor, propX, bladeY2, finColor, 1.6f * subScale, 0.0f);
    }
    C2D_DrawCircleSolid(propX, ey, 0.0f, 1.3f * subScale, finColor);
}

static void drawBackground() {
    u32 activeTheme = (settings.bgThemeIndex < 5) ? settings.bgThemeIndex : 0;

    float targetTopR = depthThemes[activeTheme].topR;
    float targetTopG = depthThemes[activeTheme].topG;
    float targetTopB = depthThemes[activeTheme].topB;
    float targetBotR = depthThemes[activeTheme].botR;
    float targetBotG = depthThemes[activeTheme].botG;
    float targetBotB = depthThemes[activeTheme].botB;

    if (settings.gradientInverted) {
        float tmpR = targetTopR, tmpG = targetTopG, tmpB = targetTopB;
        targetTopR = targetBotR; targetTopG = targetBotG; targetTopB = targetBotB;
        targetBotR = tmpR;       targetBotG = tmpG;       targetBotB = tmpB;
    }

    currentTopR += (targetTopR - currentTopR) * THEME_TRANSITION_SPEED;
    currentTopG += (targetTopG - currentTopG) * THEME_TRANSITION_SPEED;
    currentTopB += (targetTopB - currentTopB) * THEME_TRANSITION_SPEED;

    currentBotR += (targetBotR - currentBotR) * THEME_TRANSITION_SPEED;
    currentBotG += (targetBotG - currentBotG) * THEME_TRANSITION_SPEED;
    currentBotB += (targetBotB - currentBotB) * THEME_TRANSITION_SPEED;

    float drawTopR = currentTopR, drawTopG = currentTopG, drawTopB = currentTopB;
    float drawBotR = currentBotR, drawBotG = currentBotG, drawBotB = currentBotB;

    if (alarmRinging) {

        float pulse = (sinf(alarmFlashPhase) * 0.5f) + 0.5f;
        float flashR = 230.0f, flashG = 15.0f, flashB = 10.0f;

        drawTopR += (flashR - drawTopR) * pulse;
        drawTopG += (flashG - drawTopG) * pulse;
        drawTopB += (flashB - drawTopB) * pulse;
        drawBotR += (flashR - drawBotR) * pulse;
        drawBotG += (flashG - drawBotG) * pulse;
        drawBotB += (flashB - drawBotB) * pulse;
    }

    u32 topColor = C2D_Color32((u8)drawTopR, (u8)drawTopG, (u8)drawTopB, 255);
    u32 botColor = C2D_Color32((u8)drawBotR, (u8)drawBotG, (u8)drawBotB, 255);

    C2D_DrawRectangle(0, 0, 0.0f, 400, 240, topColor, topColor, botColor, botColor);
}

static void drawCaustics(float t) {
    for (int i = 0; i < CAUSTIC_COUNT; i++) {
        Caustic* c = &caustics[i];
        const int SEGMENTS = 10;
        float prevX = c->baseX, prevY = c->topY;

        for (int s = 1; s <= SEGMENTS; s++) {
            float segT  = (float)s / (float)SEGMENTS;

            float sway  = (sinf(t * c->swaySpeed + c->swayPhase + segT * 3.0f) * 0.7f
                         + sinf(t * c->swaySpeed * 1.7f + c->swayPhase * 1.3f + segT * 5.0f) * 0.3f)
                         * c->swayAmp * segT;
            float curX  = c->baseX + sway;
            float curY  = c->topY + c->length * segT;

            float fade = 1.0f - segT * 0.85f;
            u8 a = (u8)(c->alphaBase * 255.0f * fade);
            float w = c->width * (1.0f - segT * 0.6f);

            u32 rayColor = C2D_Color32(200, 235, 245, a);
            C2D_DrawLine(prevX, prevY, rayColor, curX, curY, rayColor, w, 0.0f);

            prevX = curX; prevY = curY;
        }
    }
}

static void drawClock(C2D_TextBuf buf, struct tm* tmv) {
    char timeStr[32], dateStr[32];

    if (settings.timeFormat24h) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d",
                 tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
    } else {
        int hour12 = tmv->tm_hour % 12;
        if (hour12 == 0) hour12 = 12;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d %s",
                 hour12, tmv->tm_min, tmv->tm_sec,
                 (tmv->tm_hour >= 12) ? "pm" : "am");
    }

    if (settings.dateFormat == 0)
        snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d",
                 tmv->tm_mon+1, tmv->tm_mday, tmv->tm_year+1900);
    else if (settings.dateFormat == 1)
        snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d",
                 tmv->tm_mday, tmv->tm_mon+1, tmv->tm_year+1900);
    else
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
                 tmv->tm_year+1900, tmv->tm_mon+1, tmv->tm_mday);

    C2D_Text timeText, dateText;
    C2D_TextParse(&timeText, buf, timeStr); C2D_TextOptimize(&timeText);
    C2D_TextParse(&dateText, buf, dateStr); C2D_TextOptimize(&dateText);

    float tScaleX = 1.35f, tScaleY = 1.60f;
    float dScaleX = 0.75f, dScaleY = 0.85f;

    if (settings.clockSizePreset == 1) {
        tScaleX *= 1.15f; tScaleY *= 1.15f;
        dScaleX *= 1.15f; dScaleY *= 1.15f;
    } else if (settings.clockSizePreset == 2) {
        tScaleX *= 1.30f; tScaleY *= 1.30f;
        dScaleX *= 1.30f; dScaleY *= 1.30f;
    } else if (settings.clockSizePreset == 3) {
        tScaleX *= 0.85f; tScaleY *= 0.85f;
        dScaleX *= 0.85f; dScaleY *= 0.85f;
    }

    float tw, th, dw, dh;
    C2D_TextGetDimensions(&timeText, tScaleX, tScaleY, &tw, &th);
    C2D_TextGetDimensions(&dateText, dScaleX, dScaleY, &dw, &dh);

    const float cx = 200.0f + settings.clockOffsetX;
    float timeX = cx - tw * 0.5f;
    float dateX = cx - dw * 0.5f;

    float timeY = 148.0f - th + settings.clockOffsetY;
    float dateY = 140.0f + settings.clockOffsetY;

    float targetR, targetG, targetB;

    if (settings.clockColorIndex == 15) {
        extern float globalAnimTime;
        float timeFactor = globalAnimTime * 0.25f;

        targetR = (sinf(timeFactor) * 50.0f) + 205.0f;
        targetG = (sinf(timeFactor + 2.09439f) * 50.0f) + 205.0f;
        targetB = (sinf(timeFactor + 4.18879f) * 50.0f) + 205.0f;
    } else {
        u32 targetCol = clockPresets[settings.clockColorIndex];
        targetR = (float)(targetCol & 0xFF);
        targetG = (float)((targetCol >> 8) & 0xFF);
        targetB = (float)((targetCol >> 16) & 0xFF);
    }

    currentClockR += (targetR - currentClockR) * COLOR_TRANSITION_SPEED;
    currentClockG += (targetG - currentClockG) * COLOR_TRANSITION_SPEED;
    currentClockB += (targetB - currentClockB) * COLOR_TRANSITION_SPEED;

    float drawClockR = currentClockR, drawClockG = currentClockG, drawClockB = currentClockB;
    if (alarmRinging) {
        float pulse = (sinf(alarmFlashPhase) * 0.5f) + 0.5f;
        drawClockR += (230.0f - drawClockR) * pulse;
        drawClockG += ( 15.0f - drawClockG) * pulse;
        drawClockB += ( 10.0f - drawClockB) * pulse;
    }

    u32 col    = C2D_Color32((u8)drawClockR, (u8)drawClockG, (u8)drawClockB, 255);
    u32 shadow = C2D_Color32(12, 12, 24, 220);

    const float off = 2.0f;
    C2D_DrawText(&timeText, C2D_WithColor, timeX-1+off, timeY+off,   0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timeText, C2D_WithColor, timeX+1+off, timeY+off,   0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timeText, C2D_WithColor, timeX+off,   timeY-1+off, 0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timeText, C2D_WithColor, timeX+off,   timeY+1+off, 0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timeText, C2D_WithColor, timeX+off,   timeY+off,   0.48f, tScaleX, tScaleY, shadow);

    C2D_DrawText(&timeText, C2D_WithColor, timeX-1, timeY,   0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timeText, C2D_WithColor, timeX+1, timeY,   0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timeText, C2D_WithColor, timeX,   timeY-1, 0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timeText, C2D_WithColor, timeX,   timeY+1, 0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timeText, C2D_WithColor, timeX,   timeY,   0.50f, tScaleX, tScaleY, col);

    if (settings.clockMode == 0) {
        const float doff = 1.5f;
        C2D_DrawText(&dateText, C2D_WithColor, dateX-0.8f+doff, dateY+doff, 0.48f, dScaleX, dScaleY, shadow);
        C2D_DrawText(&dateText, C2D_WithColor, dateX+0.8f+doff, dateY+doff, 0.48f, dScaleX, dScaleY, shadow);
        C2D_DrawText(&dateText, C2D_WithColor, dateX+doff,      dateY+doff, 0.48f, dScaleX, dScaleY, shadow);
        C2D_DrawText(&dateText, C2D_WithColor, dateX-0.8f, dateY, 0.50f, dScaleX, dScaleY, col);
        C2D_DrawText(&dateText, C2D_WithColor, dateX+0.8f, dateY, 0.50f, dScaleX, dScaleY, col);
        C2D_DrawText(&dateText, C2D_WithColor, dateX,      dateY, 0.50f, dScaleX, dScaleY, col);
    }
}

static void drawTimerCountdown(C2D_TextBuf buf) {
    if (timerState != TIMER_RUNNING && timerState != TIMER_PAUSED) return;

    u32 h = timerRemainingSeconds / 3600;
    u32 m = (timerRemainingSeconds % 3600) / 60;
    u32 s = timerRemainingSeconds % 60;
    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);

    C2D_Text timerText;
    C2D_TextParse(&timerText, buf, timerStr); C2D_TextOptimize(&timerText);

    float tScaleX = 1.35f, tScaleY = 1.60f;
    if (settings.clockSizePreset == 1)      { tScaleX *= 1.15f; tScaleY *= 1.15f; }
    else if (settings.clockSizePreset == 2) { tScaleX *= 1.30f; tScaleY *= 1.30f; }
    else if (settings.clockSizePreset == 3) { tScaleX *= 0.85f; tScaleY *= 0.85f; }

    float tw, th;
    C2D_TextGetDimensions(&timerText, tScaleX, tScaleY, &tw, &th);

    const float cx = 200.0f + settings.clockOffsetX;
    float timerX = cx - tw * 0.5f;

    float timerY = 165.0f + ((240.0f - 165.0f) - th) * 0.5f + settings.clockOffsetY;

    float drawR = currentClockR, drawG = currentClockG, drawB = currentClockB;
    if (alarmRinging) {
        float pulse = (sinf(alarmFlashPhase) * 0.5f) + 0.5f;
        drawR += (230.0f - drawR) * pulse;
        drawG += ( 15.0f - drawG) * pulse;
        drawB += ( 10.0f - drawB) * pulse;
    }
    u32 col    = C2D_Color32((u8)drawR, (u8)drawG, (u8)drawB, 255);
    u32 shadow = C2D_Color32(12, 12, 24, 220);

    const float off = 2.0f;
    C2D_DrawText(&timerText, C2D_WithColor, timerX-1+off, timerY+off,   0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timerText, C2D_WithColor, timerX+1+off, timerY+off,   0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timerText, C2D_WithColor, timerX+off,   timerY-1+off, 0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timerText, C2D_WithColor, timerX+off,   timerY+1+off, 0.48f, tScaleX, tScaleY, shadow);
    C2D_DrawText(&timerText, C2D_WithColor, timerX+off,   timerY+off,   0.48f, tScaleX, tScaleY, shadow);

    C2D_DrawText(&timerText, C2D_WithColor, timerX-1, timerY,   0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timerText, C2D_WithColor, timerX+1, timerY,   0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timerText, C2D_WithColor, timerX,   timerY-1, 0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timerText, C2D_WithColor, timerX,   timerY+1, 0.50f, tScaleX, tScaleY, col);
    C2D_DrawText(&timerText, C2D_WithColor, timerX,   timerY,   0.50f, tScaleX, tScaleY, col);
}

float globalAnimTime = 0.0f;

static void drawBottomScreen(C2D_TextBuf buf, C3D_RenderTarget* target) {
    C2D_SceneBegin(target);

    if (lowerScreenOff) {
        C2D_TargetClear(target, C2D_Color32(0, 0, 0, 255));

        updateAndDrawBottomJellies(globalAnimTime, 0.0f, false);
        updateAndDrawHourSchoolFormation(globalAnimTime, false, false);
        return;
    }

    u32 colTextBlack = C2D_Color32( 15,  15,  20, 255);
    u32 colTextWhite = C2D_Color32(255, 255, 255, 255);
    u32 colWhite     = C2D_Color32(230, 235, 245, 255);
    u32 colLightGreen= C2D_Color32( 50, 210, 100, 255);
    u32 colCyan      = C2D_Color32(  0, 191, 255, 255);
    u32 colRed       = C2D_Color32(220,  40,  40, 255);

    if (currentScreen == SCREEN_MAIN) {

        u32 bottomBgColor = C2D_Color32((u8)currentBotR, (u8)currentBotG, (u8)currentBotB, 255);
        C2D_TargetClear(target, bottomBgColor);

        updateAndDrawBottomBubbles(globalAnimTime);

        updateAndDrawHourSchoolFormation(globalAnimTime, false, true);
        C2D_DrawRectSolid(10, 10,  0.0f, 300, 2, C2D_Color32(60, 130, 145, 255));
        C2D_DrawRectSolid(10, 228, 0.0f, 300, 2, C2D_Color32(60, 130, 145, 255));

        float tw, th;
        const float titleScale = 0.75f * 1.4f;
        C2D_TextGetDimensions(&ui.title, titleScale, titleScale, &tw, &th);
        {

            float titleX = (320.0f - tw) * 0.5f;
            float titleY = 20.0f;
            u32 titleCol    = C2D_Color32(60, 130, 145, 255);
            u32 titleShadow = C2D_Color32(12, 12, 24, 220);
            const float off = 2.0f;

            C2D_DrawText(&ui.title, C2D_WithColor, titleX-1+off, titleY+off,   0.48f, titleScale, titleScale, titleShadow);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX+1+off, titleY+off,   0.48f, titleScale, titleScale, titleShadow);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX+off,   titleY-1+off, 0.48f, titleScale, titleScale, titleShadow);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX+off,   titleY+1+off, 0.48f, titleScale, titleScale, titleShadow);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX+off,   titleY+off,   0.48f, titleScale, titleScale, titleShadow);

            C2D_DrawText(&ui.title, C2D_WithColor, titleX-1, titleY,   0.5f, titleScale, titleScale, titleCol);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX+1, titleY,   0.5f, titleScale, titleScale, titleCol);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX,   titleY-1, 0.5f, titleScale, titleScale, titleCol);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX,   titleY+1, 0.5f, titleScale, titleScale, titleCol);
            C2D_DrawText(&ui.title, C2D_WithColor, titleX,   titleY,   0.5f, titleScale, titleScale, titleCol);
        }

        updateAndDrawBottomJellies(globalAnimTime, 0.6f, true);

        if (alarmCfg.enabled) {
            char mainAlarmStr[48];
            if (!settings.timeFormat24h) {
                u32 h12 = alarmCfg.hour % 12; if (h12 == 0) h12 = 12;
                snprintf(mainAlarmStr, sizeof(mainAlarmStr), "Alarm set: %02u:%02u %s",
                         (unsigned)h12, (unsigned)alarmCfg.minute, (alarmCfg.hour >= 12) ? "pm" : "am");
            } else {
                snprintf(mainAlarmStr, sizeof(mainAlarmStr), "Alarm set: %02u:%02u", (unsigned)alarmCfg.hour, (unsigned)alarmCfg.minute);
            }
            C2D_Text tMainAlarm; C2D_TextParse(&tMainAlarm, buf, mainAlarmStr); C2D_TextOptimize(&tMainAlarm);
            C2D_TextGetDimensions(&tMainAlarm, 0.45f, 0.45f, &tw, &th);
            C2D_DrawText(&tMainAlarm, C2D_WithColor, (320.0f-tw)*0.5f, 53, 0.5f, 0.45f, 0.45f, C2D_Color32(240,180,100,255));
        }

        float bw, bh;
        u32 colOceanSoft = C2D_Color32(60, 130, 145, 255);

        C2D_DrawRectSolid(15, 185, 0.7f, 90, 32, colOceanSoft);
        C2D_TextGetDimensions(&ui.btnSettings, 0.55f, 0.55f, &bw, &bh);
        C2D_DrawText(&ui.btnSettings, C2D_WithColor, 15+(90-bw)*0.5f, 185+(32-bh)*0.5f, 0.7f, 0.55f, 0.55f, colTextWhite);

        C2D_DrawRectSolid(115, 185, 0.7f, 90, 32, colRed);
        C2D_TextGetDimensions(&ui.btnOff, 0.60f, 0.60f, &bw, &bh);
        C2D_DrawText(&ui.btnOff, C2D_WithColor, 115+(90-bw)*0.5f, 185+(32-bh)*0.5f, 0.7f, 0.60f, 0.60f, colTextWhite);

        C2D_DrawRectSolid(215, 185, 0.7f, 90, 32, colOceanSoft);
        C2D_TextGetDimensions(&ui.btnRecords, 0.55f, 0.55f, &bw, &bh);
        C2D_DrawText(&ui.btnRecords, C2D_WithColor, 215+(90-bw)*0.5f, 185+(32-bh)*0.5f, 0.7f, 0.55f, 0.55f, colTextWhite);
    }
    else if (currentScreen == SCREEN_RECORDS) {

        u32 bottomBgColor = C2D_Color32((u8)currentBotR, (u8)currentBotG, (u8)currentBotB, 255);
        C2D_TargetClear(target, bottomBgColor);
        updateAndDrawBottomBubbles(globalAnimTime);
        updateAndDrawBottomJellies(globalAnimTime, 0.0f, true);
        C2D_DrawRectSolid(10, 10,  0.0f, 300, 2, C2D_Color32(80, 50, 60, 255));
        C2D_DrawRectSolid(10, 228, 0.0f, 300, 2, C2D_Color32(80, 50, 60, 255));

        float tw, th;
        C2D_TextGetDimensions(&ui.hintMove, 0.50f, 0.50f, &tw, &th);
        C2D_DrawText(&ui.hintMove, C2D_WithColor, (320.0f-tw)*0.5f, 28, 0.5f, 0.50f, 0.50f, C2D_Color32(180,180,180,255));

        C2D_TextGetDimensions(&ui.hintCenter, 0.50f, 0.50f, &tw, &th);
        C2D_DrawText(&ui.hintCenter, C2D_WithColor, (320.0f-tw)*0.5f, 52, 0.5f, 0.50f, 0.50f, C2D_Color32(180,180,180,255));

        C2D_TextGetDimensions(&ui.hintSelect, 0.50f, 0.50f, &tw, &th);
        C2D_DrawText(&ui.hintSelect, C2D_WithColor, (320.0f-tw)*0.5f, 78, 0.5f, 0.50f, 0.50f, C2D_Color32(180,180,180,255));

        C2D_TextGetDimensions(&ui.hintSize, 0.50f, 0.50f, &tw, &th);
        C2D_DrawText(&ui.hintSize, C2D_WithColor, (320.0f-tw)*0.5f, 102, 0.5f, 0.50f, 0.50f, C2D_Color32(180,180,180,255));

        C2D_TextGetDimensions(&ui.hintStart, 0.50f, 0.50f, &tw, &th);
        C2D_DrawText(&ui.hintStart, C2D_WithColor, (320.0f-tw)*0.5f, 128, 0.5f, 0.50f, 0.50f, C2D_Color32(180,180,180,255));

        C2D_TextGetDimensions(&ui.hintOff, 0.50f, 0.50f, &tw, &th);
        C2D_DrawText(&ui.hintOff, C2D_WithColor, (320.0f-tw)*0.5f, 152, 0.5f, 0.50f, 0.50f, C2D_Color32(180,180,180,255));

        float bw, bh;
        u32 colOceanSoftRec = C2D_Color32(60, 130, 145, 255);

        C2D_DrawRectSolid(15, 185, 0.0f, 142, 32, colOceanSoftRec);
        C2D_TextGetDimensions(&ui.btnCredits, 0.50f, 0.50f, &bw, &bh);
        C2D_DrawText(&ui.btnCredits, C2D_WithColor, 15+(142-bw)*0.5f, 185+(32-bh)*0.5f, 0.5f, 0.50f, 0.50f, colTextWhite);

        C2D_DrawRectSolid(163, 185, 0.0f, 142, 32, colOceanSoftRec);
        C2D_TextGetDimensions(&ui.btnBack, 0.50f, 0.50f, &bw, &bh);
        C2D_DrawText(&ui.btnBack, C2D_WithColor, 163+(142-bw)*0.5f, 185+(32-bh)*0.5f, 0.5f, 0.50f, 0.50f, colTextWhite);
    }
    else if (currentScreen == SCREEN_SETTINGS) {

        u32 bottomBgColor = C2D_Color32((u8)currentBotR, (u8)currentBotG, (u8)currentBotB, 255);
        C2D_TargetClear(target, bottomBgColor);
        updateAndDrawBottomBubbles(globalAnimTime);
        updateAndDrawBottomJellies(globalAnimTime, 0.0f, true);
        C2D_DrawRectSolid(10, 10,  0.0f, 300, 2, C2D_Color32(50, 80, 50, 255));
        C2D_DrawRectSolid(10, 228, 0.0f, 300, 2, C2D_Color32(50, 80, 50, 255));

        C2D_DrawText(&ui.setTitle, C2D_WithColor, 20, 15, 0.5f, 0.65f, 0.65f, colLightGreen);

        char colStr[64]; snprintf(colStr, sizeof(colStr), "Clock Color: <%s>", presetNames[settings.clockColorIndex]);
        C2D_Text tCol; C2D_TextParse(&tCol, buf, colStr); C2D_TextOptimize(&tCol);
        C2D_DrawRectSolid(15, 45, 0.0f, 290, 26, C2D_Color32(45, 55, 45, 255));

        u32 previewCol;
        if (settings.clockColorIndex == 15) {
            float menuFactor = globalAnimTime * 0.25f;
            u8 pr = (u8)((sinf(menuFactor) * 50.0f) + 205.0f);
            u8 pg = (u8)((sinf(menuFactor + 2.09439f) * 50.0f) + 205.0f);
            u8 pb = (u8)((sinf(menuFactor + 4.18879f) * 50.0f) + 205.0f);
            previewCol = C2D_Color32(pr, pg, pb, 255);
        } else {
            previewCol = C2D_Color32((u8)currentClockR, (u8)currentClockG, (u8)currentClockB, 255);
        }
        C2D_DrawText(&tCol, C2D_WithColor, 25, 50, 0.5f, 0.50f, 0.50f, previewCol);

        u32 themeIndex = (settings.bgThemeIndex < 5) ? settings.bgThemeIndex : 0;
        char themeStr[64]; snprintf(themeStr, sizeof(themeStr), "Depth Theme: <%s>", depthThemes[themeIndex].name);
        C2D_Text tTheme; C2D_TextParse(&tTheme, buf, themeStr); C2D_TextOptimize(&tTheme);

        C2D_DrawRectSolid(15, 78, 0.0f, 240, 26, C2D_Color32(45, 55, 45, 255));

        u32 targetThemeCol = depthThemes[themeIndex].textMenuColor;
        float targetTR = (float)(targetThemeCol & 0xFF);
        float targetTG = (float)((targetThemeCol >> 8) & 0xFF);
        float targetTB = (float)((targetThemeCol >> 16) & 0xFF);

        currentThemeTextR += (targetTR - currentThemeTextR) * COLOR_TRANSITION_SPEED;
        currentThemeTextG += (targetTG - currentThemeTextG) * COLOR_TRANSITION_SPEED;
        currentThemeTextB += (targetTB - currentThemeTextB) * COLOR_TRANSITION_SPEED;

        u32 finalThemeTxtCol = C2D_Color32((u8)currentThemeTextR, (u8)currentThemeTextG, (u8)currentThemeTextB, 255);
        C2D_DrawText(&tTheme, C2D_WithColor, 25, 83, 0.5f, 0.50f, 0.50f, finalThemeTxtCol);

        u32 invertBtnCol = settings.gradientInverted ? colLightGreen : C2D_Color32(70, 80, 70, 255);
        u32 invertTxtCol = settings.gradientInverted ? colTextBlack  : C2D_Color32(180, 190, 180, 255);
        C2D_DrawRectSolid(261, 78, 0.0f, 44, 26, invertBtnCol);
        char invertStr[16]; snprintf(invertStr, sizeof(invertStr), "Invert");
        C2D_Text tInvert; C2D_TextParse(&tInvert, buf, invertStr); C2D_TextOptimize(&tInvert);
        float ibw, ibh;
        C2D_TextGetDimensions(&tInvert, 0.42f, 0.42f, &ibw, &ibh);
        C2D_DrawText(&tInvert, C2D_WithColor, 261+(44-ibw)*0.5f, 78+(26-ibh)*0.5f, 0.5f, 0.42f, 0.42f, invertTxtCol);

        char timeFmtStr[64]; snprintf(timeFmtStr, sizeof(timeFmtStr), "Time Format: <%s>", settings.timeFormat24h ? "24 Hours" : "12 Hours (am/pm)");
        C2D_Text tTF; C2D_TextParse(&tTF, buf, timeFmtStr); C2D_TextOptimize(&tTF);
        C2D_DrawRectSolid(15, 111, 0.0f, 290, 26, C2D_Color32(45, 55, 45, 255));
        C2D_DrawText(&tTF, C2D_WithColor, 25, 116, 0.5f, 0.50f, 0.50f, colWhite);

        char dateFmtStr[64]; snprintf(dateFmtStr, sizeof(dateFmtStr), "Date Format: <%s>", settings.dateFormat == 2 ? "YYYY-MM-DD (ISO)" : (settings.dateFormat ? "DD/MM/YYYY (EU)" : "MM/DD/YYYY (USA)"));
        C2D_Text tDF; C2D_TextParse(&tDF, buf, dateFmtStr); C2D_TextOptimize(&tDF);
        C2D_DrawRectSolid(15, 144, 0.0f, 290, 26, C2D_Color32(45, 55, 45, 255));
        C2D_DrawText(&tDF, C2D_WithColor, 25, 149, 0.5f, 0.50f, 0.50f, colWhite);

        float bw, bh;
        u32 colOceanSoftSet = C2D_Color32(60, 130, 145, 255);

        C2D_DrawRectSolid(15,  190, 0.0f, 69, 30, colOceanSoftSet);
        C2D_TextGetDimensions(&ui.btnBackSet, 0.50f, 0.50f, &bw, &bh);
        C2D_DrawText(&ui.btnBackSet, C2D_WithColor, 15+(69-bw)*0.5f, 190+(30-bh)*0.5f, 0.5f, 0.50f, 0.50f, colTextWhite);

        C2D_DrawRectSolid(89, 190, 0.0f, 69, 30, colOceanSoftSet);
        C2D_TextGetDimensions(&ui.btnAlarmSet, 0.50f, 0.50f, &bw, &bh);
        C2D_DrawText(&ui.btnAlarmSet, C2D_WithColor, 89+(69-bw)*0.5f, 190+(30-bh)*0.5f, 0.5f, 0.50f, 0.50f, colTextWhite);

        C2D_DrawRectSolid(162, 190, 0.0f, 69, 30, colOceanSoftSet);
        C2D_TextGetDimensions(&ui.btnTimerSet, 0.50f, 0.50f, &bw, &bh);
        C2D_DrawText(&ui.btnTimerSet, C2D_WithColor, 162+(69-bw)*0.5f, 190+(30-bh)*0.5f, 0.5f, 0.50f, 0.50f, colTextWhite);

        C2D_DrawRectSolid(236, 190, 0.0f, 69, 30, colOceanSoftSet);
        C2D_TextGetDimensions(&ui.btnResetSet, 0.50f, 0.50f, &bw, &bh);
        C2D_DrawText(&ui.btnResetSet, C2D_WithColor, 236+(69-bw)*0.5f, 190+(30-bh)*0.5f, 0.5f, 0.50f, 0.50f, colTextWhite);
    }
    else if (currentScreen == SCREEN_CREDITS) {
        C2D_TargetClear(target, C2D_Color32(20, 25, 35, 255));
        C2D_DrawRectSolid(10, 10,  0.0f, 300, 2, C2D_Color32(40, 60, 90, 255));
        C2D_DrawRectSolid(10, 228, 0.0f, 300, 2, C2D_Color32(40, 60, 90, 255));

        float tw, th;
        C2D_TextGetDimensions(&ui.credTitle, 0.70f, 0.70f, &tw, &th);
        C2D_DrawText(&ui.credTitle, C2D_WithColor, (320.0f-tw)*0.5f, 25, 0.5f, 0.70f, 0.70f, colCyan);

        C2D_DrawText(&ui.credLine1, C2D_WithColor, 25, 75,  0.5f, 0.52f, 0.52f, colWhite);
        C2D_DrawText(&ui.credLine2, C2D_WithColor, 25, 100, 0.5f, 0.52f, 0.52f, colWhite);
        C2D_DrawText(&ui.credLine3, C2D_WithColor, 25, 130, 0.5f, 0.52f, 0.52f, colWhite);
        C2D_DrawText(&ui.credLine4, C2D_WithColor, 25, 155, 0.5f, 0.52f, 0.52f, colWhite);

        float bw, bh;
        u32 colOceanSoftCred = C2D_Color32(60, 130, 145, 255);
        C2D_DrawRectSolid(105, 185, 0.0f, 110, 32, colOceanSoftCred);
        C2D_TextGetDimensions(&ui.btnBackCred, 0.6f, 0.6f, &bw, &bh);
        C2D_DrawText(&ui.btnBackCred, C2D_WithColor, 105+(110-bw)*0.5f, 185+(32-bh)*0.5f, 0.5f, 0.6f, 0.6f, colTextWhite);
    }
    else if (currentScreen == SCREEN_ALARM) {
        C2D_TargetClear(target, C2D_Color32(25, 30, 38, 255));
        C2D_DrawRectSolid(10, 10,  0.0f, 300, 2, C2D_Color32(60, 80, 95, 255));
        C2D_DrawRectSolid(10, 228, 0.0f, 300, 2, C2D_Color32(60, 80, 95, 255));

        bool is12h = !settings.timeFormat24h;

        float tw, th;
        C2D_TextGetDimensions(&ui.alarmTitle, 0.70f, 0.70f, &tw, &th);
        C2D_DrawText(&ui.alarmTitle, C2D_WithColor, (320.0f-tw)*0.5f, 22, 0.5f, 0.70f, 0.70f, C2D_Color32(110,180,230,255));

        char statusStr[48];
        if (alarmCfg.enabled) {
            if (is12h) {
                u32 h12 = alarmCfg.hour % 12; if (h12 == 0) h12 = 12;
                snprintf(statusStr, sizeof(statusStr), "Alarm set: %02u:%02u %s",
                         (unsigned)h12, (unsigned)alarmCfg.minute, (alarmCfg.hour >= 12) ? "pm" : "am");
            } else {
                snprintf(statusStr, sizeof(statusStr), "Alarm set: %02u:%02u", (unsigned)alarmCfg.hour, (unsigned)alarmCfg.minute);
            }
        } else {
            snprintf(statusStr, sizeof(statusStr), "Alarm is OFF");
        }
        C2D_Text tStatus; C2D_TextParse(&tStatus, buf, statusStr); C2D_TextOptimize(&tStatus);
        C2D_TextGetDimensions(&tStatus, 0.45f, 0.45f, &tw, &th);
        C2D_DrawText(&tStatus, C2D_WithColor, (320.0f-tw)*0.5f, 55, 0.5f, 0.45f, 0.45f, colWhite);

        float hourFieldX, minFieldX, ampmFieldX;
        if (is12h) {
            const float gap = 10.0f;
            const float totalW = 80.0f + gap + 80.0f + gap + 66.0f;
            const float startX = 15.0f + (290.0f - totalW) * 0.5f;
            hourFieldX = startX;
            minFieldX  = hourFieldX + 80.0f + gap;
            ampmFieldX = minFieldX  + 80.0f + gap;
        } else {
            hourFieldX = 82.0f;
            minFieldX  = 172.0f;
            ampmFieldX = 0.0f;
        }

        C2D_DrawRectSolid(hourFieldX, 80, 0.0f, 80, 70, C2D_Color32(40, 48, 58, 255));
        C2D_TextGetDimensions(&ui.alarmHourLabel, 0.45f, 0.45f, &tw, &th);
        C2D_DrawText(&ui.alarmHourLabel, C2D_WithColor, hourFieldX+(80-tw)*0.5f, 86, 0.5f, 0.45f, 0.45f, C2D_Color32(150,170,190,255));

        char hourStr[8]; snprintf(hourStr, sizeof(hourStr), "%02u", (unsigned)alarmEditHour);
        C2D_Text tHour; C2D_TextParse(&tHour, buf, hourStr); C2D_TextOptimize(&tHour);
        C2D_TextGetDimensions(&tHour, 0.9f, 0.9f, &tw, &th);
        C2D_DrawText(&tHour, C2D_WithColor, hourFieldX+(80-tw)*0.5f, 108, 0.5f, 0.9f, 0.9f, colTextWhite);

        C2D_DrawRectSolid(minFieldX, 80, 0.0f, 80, 70, C2D_Color32(40, 48, 58, 255));
        C2D_TextGetDimensions(&ui.alarmMinLabel, 0.45f, 0.45f, &tw, &th);
        C2D_DrawText(&ui.alarmMinLabel, C2D_WithColor, minFieldX+(80-tw)*0.5f, 86, 0.5f, 0.45f, 0.45f, C2D_Color32(150,170,190,255));

        char minStr[8]; snprintf(minStr, sizeof(minStr), "%02u", (unsigned)alarmEditMinute);
        C2D_Text tMin; C2D_TextParse(&tMin, buf, minStr); C2D_TextOptimize(&tMin);
        C2D_TextGetDimensions(&tMin, 0.9f, 0.9f, &tw, &th);
        C2D_DrawText(&tMin, C2D_WithColor, minFieldX+(80-tw)*0.5f, 108, 0.5f, 0.9f, 0.9f, colTextWhite);

        if (is12h) {
            C2D_DrawRectSolid(ampmFieldX, 91, 0.0f, 66, 48, C2D_Color32(40, 48, 58, 255));
            char ampmStr[4]; snprintf(ampmStr, sizeof(ampmStr), "%s", alarmEditPM ? "PM" : "AM");
            C2D_Text tAmpm; C2D_TextParse(&tAmpm, buf, ampmStr); C2D_TextOptimize(&tAmpm);
            C2D_TextGetDimensions(&tAmpm, 0.72f, 0.72f, &tw, &th);
            C2D_DrawText(&tAmpm, C2D_WithColor, ampmFieldX+(66-tw)*0.5f, 91+(48-th)*0.5f, 0.5f, 0.72f, 0.72f, colTextWhite);
        }

        float abw, abh;
        u32 colOceanSoftAlarm = C2D_Color32(60, 130, 145, 255);

        C2D_DrawRectSolid(15,  190, 0.0f, 90, 30, colOceanSoftAlarm);
        C2D_TextGetDimensions(&ui.btnAlarmBack, 0.55f, 0.55f, &abw, &abh);
        C2D_DrawText(&ui.btnAlarmBack, C2D_WithColor, 15+(90-abw)*0.5f, 190+(30-abh)*0.5f, 0.5f, 0.55f, 0.55f, colTextWhite);

        C2D_DrawRectSolid(115, 190, 0.0f, 90, 30, colOceanSoftAlarm);
        C2D_TextGetDimensions(&ui.btnAlarmSetConfirm, 0.55f, 0.55f, &abw, &abh);
        C2D_DrawText(&ui.btnAlarmSetConfirm, C2D_WithColor, 115+(90-abw)*0.5f, 190+(30-abh)*0.5f, 0.5f, 0.55f, 0.55f, colTextWhite);

        C2D_DrawRectSolid(215, 190, 0.0f, 90, 30, colOceanSoftAlarm);
        C2D_TextGetDimensions(&ui.btnAlarmClear, 0.55f, 0.55f, &abw, &abh);
        C2D_DrawText(&ui.btnAlarmClear, C2D_WithColor, 215+(90-abw)*0.5f, 190+(30-abh)*0.5f, 0.5f, 0.55f, 0.55f, colTextWhite);
    }
    else if (currentScreen == SCREEN_TIMER) {
        C2D_TargetClear(target, C2D_Color32(25, 30, 38, 255));
        C2D_DrawRectSolid(10, 10,  0.0f, 300, 2, C2D_Color32(60, 80, 95, 255));
        C2D_DrawRectSolid(10, 228, 0.0f, 300, 2, C2D_Color32(60, 80, 95, 255));

        float tw, th;
        C2D_TextGetDimensions(&ui.timerTitle, 0.70f, 0.70f, &tw, &th);
        C2D_DrawText(&ui.timerTitle, C2D_WithColor, (320.0f-tw)*0.5f, 22, 0.5f, 0.70f, 0.70f, C2D_Color32(110,180,230,255));

        char statusStr[48];
        if (timerState == TIMER_RUNNING || timerState == TIMER_PAUSED) {
            u32 h = timerRemainingSeconds / 3600;
            u32 m = (timerRemainingSeconds % 3600) / 60;
            u32 s = timerRemainingSeconds % 60;
            snprintf(statusStr, sizeof(statusStr), "%s: %02u:%02u:%02u",
                     (timerState == TIMER_PAUSED) ? "Paused" : "Running", (unsigned)h, (unsigned)m, (unsigned)s);
        } else {
            snprintf(statusStr, sizeof(statusStr), "Timer not set");
        }
        C2D_Text tStatus; C2D_TextParse(&tStatus, buf, statusStr); C2D_TextOptimize(&tStatus);
        C2D_TextGetDimensions(&tStatus, 0.45f, 0.45f, &tw, &th);
        C2D_DrawText(&tStatus, C2D_WithColor, (320.0f-tw)*0.5f, 55, 0.5f, 0.45f, 0.45f, colWhite);

        const float fieldW = 80.0f, gap = 10.0f;
        const float totalW = fieldW*3 + gap*2;
        const float startX = 15.0f + (290.0f - totalW) * 0.5f;
        float hourFieldX = startX;
        float minFieldX  = hourFieldX + fieldW + gap;
        float secFieldX  = minFieldX  + fieldW + gap;

        bool fieldsEditable = (timerState == TIMER_IDLE);
        u32 fieldBgCol = fieldsEditable ? C2D_Color32(40, 48, 58, 255) : C2D_Color32(30, 34, 40, 255);
        u32 fieldTxtCol = fieldsEditable ? colTextWhite : C2D_Color32(140, 145, 150, 255);

        C2D_DrawRectSolid(hourFieldX, 80, 0.0f, fieldW, 70, fieldBgCol);
        C2D_TextGetDimensions(&ui.timerHourLabel, 0.42f, 0.42f, &tw, &th);
        C2D_DrawText(&ui.timerHourLabel, C2D_WithColor, hourFieldX+(fieldW-tw)*0.5f, 86, 0.5f, 0.42f, 0.42f, C2D_Color32(150,170,190,255));
        char hourStr[8]; snprintf(hourStr, sizeof(hourStr), "%02u", (unsigned)timerEditHour);
        C2D_Text tHour; C2D_TextParse(&tHour, buf, hourStr); C2D_TextOptimize(&tHour);
        C2D_TextGetDimensions(&tHour, 0.85f, 0.85f, &tw, &th);
        C2D_DrawText(&tHour, C2D_WithColor, hourFieldX+(fieldW-tw)*0.5f, 108, 0.5f, 0.85f, 0.85f, fieldTxtCol);

        C2D_DrawRectSolid(minFieldX, 80, 0.0f, fieldW, 70, fieldBgCol);
        C2D_TextGetDimensions(&ui.timerMinLabel, 0.42f, 0.42f, &tw, &th);
        C2D_DrawText(&ui.timerMinLabel, C2D_WithColor, minFieldX+(fieldW-tw)*0.5f, 86, 0.5f, 0.42f, 0.42f, C2D_Color32(150,170,190,255));
        char minStr[8]; snprintf(minStr, sizeof(minStr), "%02u", (unsigned)timerEditMinute);
        C2D_Text tMin; C2D_TextParse(&tMin, buf, minStr); C2D_TextOptimize(&tMin);
        C2D_TextGetDimensions(&tMin, 0.85f, 0.85f, &tw, &th);
        C2D_DrawText(&tMin, C2D_WithColor, minFieldX+(fieldW-tw)*0.5f, 108, 0.5f, 0.85f, 0.85f, fieldTxtCol);

        C2D_DrawRectSolid(secFieldX, 80, 0.0f, fieldW, 70, fieldBgCol);
        C2D_TextGetDimensions(&ui.timerSecLabel, 0.42f, 0.42f, &tw, &th);
        C2D_DrawText(&ui.timerSecLabel, C2D_WithColor, secFieldX+(fieldW-tw)*0.5f, 86, 0.5f, 0.42f, 0.42f, C2D_Color32(150,170,190,255));
        char secStr[8]; snprintf(secStr, sizeof(secStr), "%02u", (unsigned)timerEditSecond);
        C2D_Text tSec; C2D_TextParse(&tSec, buf, secStr); C2D_TextOptimize(&tSec);
        C2D_TextGetDimensions(&tSec, 0.85f, 0.85f, &tw, &th);
        C2D_DrawText(&tSec, C2D_WithColor, secFieldX+(fieldW-tw)*0.5f, 108, 0.5f, 0.85f, 0.85f, fieldTxtCol);

        float tbw, tbh;
        u32 colOceanSoftTimer = C2D_Color32(60, 130, 145, 255);

        C2D_DrawRectSolid(15,  190, 0.0f, 90, 30, colOceanSoftTimer);
        C2D_TextGetDimensions(&ui.btnTimerBack, 0.55f, 0.55f, &tbw, &tbh);
        C2D_DrawText(&ui.btnTimerBack, C2D_WithColor, 15+(90-tbw)*0.5f, 190+(30-tbh)*0.5f, 0.5f, 0.55f, 0.55f, colTextWhite);

        C2D_Text* centerBtn;
        C2D_Text* rightBtn;
        if (timerState == TIMER_IDLE)         { centerBtn = &ui.btnTimerStart;  rightBtn = &ui.btnTimerReset; }
        else if (timerState == TIMER_RUNNING) { centerBtn = &ui.btnTimerPause;  rightBtn = &ui.btnTimerStop;  }
        else                { centerBtn = &ui.btnTimerResume; rightBtn = &ui.btnTimerStop;  }

        C2D_DrawRectSolid(115, 190, 0.0f, 90, 30, colOceanSoftTimer);
        C2D_TextGetDimensions(centerBtn, 0.55f, 0.55f, &tbw, &tbh);
        C2D_DrawText(centerBtn, C2D_WithColor, 115+(90-tbw)*0.5f, 190+(30-tbh)*0.5f, 0.5f, 0.55f, 0.55f, colTextWhite);

        C2D_DrawRectSolid(215, 190, 0.0f, 90, 30, colOceanSoftTimer);
        C2D_TextGetDimensions(rightBtn, 0.55f, 0.55f, &tbw, &tbh);
        C2D_DrawText(rightBtn, C2D_WithColor, 215+(90-tbw)*0.5f, 190+(30-tbh)*0.5f, 0.5f, 0.55f, 0.55f, colTextWhite);
    }

    if (alarmRinging) {
        float pulse = (sinf(alarmFlashPhase) * 0.5f) + 0.5f;
        u8 popR = (u8)(180.0f + pulse * 60.0f);
        u32 popupBg = C2D_Color32(popR, 25, 20, 255);

        C2D_TargetClear(target, popupBg);

        char popStr[40] = "Tap to stop alarm";
        C2D_Text tPopup; C2D_TextParse(&tPopup, buf, popStr); C2D_TextOptimize(&tPopup);
        float pw, ph;
        C2D_TextGetDimensions(&tPopup, 0.9f, 0.9f, &pw, &ph);
        C2D_DrawText(&tPopup, C2D_WithColor, (320.0f-pw)*0.5f, (240.0f-ph)*0.5f, 0.5f, 0.9f, 0.9f, C2D_Color32(255,255,255,255));
    }
}

static int updateInput(C2D_TextBuf buf) {
    hidScanInput();
    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();

    if (kHeld & KEY_L) {
        circlePosition circlePos;
        hidCircleRead(&circlePos);

        const s16 deadzone = 12;
        if (circlePos.dx > deadzone || circlePos.dx < -deadzone) {
            settings.clockOffsetX += (float)circlePos.dx / 60.0f;
            settingsDirty = true;
        }
        if (circlePos.dy > deadzone || circlePos.dy < -deadzone) {

            settings.clockOffsetY -= (float)circlePos.dy / 60.0f;
            settingsDirty = true;
        }

        {
            const float SAFETY_MARGIN = 3.0f;

            float tScaleX = 1.35f, tScaleY = 1.60f;
            float dScaleX = 0.75f, dScaleY = 0.85f;
            if (settings.clockSizePreset == 1)      { tScaleX *= 1.15f; tScaleY *= 1.15f; dScaleX *= 1.15f; dScaleY *= 1.15f; }
            else if (settings.clockSizePreset == 2) { tScaleX *= 1.30f; tScaleY *= 1.30f; dScaleX *= 1.30f; dScaleY *= 1.30f; }
            else if (settings.clockSizePreset == 3) { tScaleX *= 0.85f; tScaleY *= 0.85f; dScaleX *= 0.85f; dScaleY *= 0.85f; }

            const char* timePlaceholder = settings.timeFormat24h ? "88:88:88" : "88:88:88 pm";
            const char* datePlaceholder = "88/88/8888";

            C2D_Text tPlaceholder, dPlaceholder;
            C2D_TextParse(&tPlaceholder, buf, timePlaceholder); C2D_TextOptimize(&tPlaceholder);
            C2D_TextParse(&dPlaceholder, buf, datePlaceholder); C2D_TextOptimize(&dPlaceholder);

            float tw, th, dw, dh;
            C2D_TextGetDimensions(&tPlaceholder, tScaleX, tScaleY, &tw, &th);
            C2D_TextGetDimensions(&dPlaceholder, dScaleX, dScaleY, &dw, &dh);

            float maxHalfWidth = (tw > dw ? tw : dw) * 0.5f;
            float maxOffsetX = 200.0f - SAFETY_MARGIN - maxHalfWidth;
            float minOffsetX = -maxOffsetX;

            const float NO_DATE_SAFETY_MARGIN = 2.0f;
            float minOffsetY = SAFETY_MARGIN - 148.0f + th;
            float maxOffsetY = (settings.clockMode == 0)
                                  ? (240.0f - SAFETY_MARGIN - 140.0f - dh)
                                  : (240.0f - NO_DATE_SAFETY_MARGIN - 148.0f);

            if (settings.clockOffsetX < minOffsetX) settings.clockOffsetX = minOffsetX;
            if (settings.clockOffsetX > maxOffsetX) settings.clockOffsetX = maxOffsetX;
            if (settings.clockOffsetY < minOffsetY) settings.clockOffsetY = minOffsetY;
            if (settings.clockOffsetY > maxOffsetY) settings.clockOffsetY = maxOffsetY;
        }

        if (kDown & KEY_A) {
            settings.clockOffsetX = 0.0f;
            settings.clockOffsetY = 0.0f;
            settingsDirty = true;
            return 0;
        }
    }

    if (kDown & KEY_A) {
        if (alarmRinging) {
            stopAlarmRinging();
            return 0;
        }
    }

    if (kDown & KEY_B) {
        if (currentScreen == SCREEN_SETTINGS) {
            currentScreen = SCREEN_MAIN;
            return 0;
        }
        else if (currentScreen == SCREEN_RECORDS) {
            currentScreen = SCREEN_MAIN;
            return 0;
        }
        else if (currentScreen == SCREEN_CREDITS) {
            currentScreen = SCREEN_RECORDS;
            return 0;
        }
        else if (currentScreen == SCREEN_ALARM) {
            currentScreen = SCREEN_SETTINGS;
            return 0;
        }
        else if (currentScreen == SCREEN_TIMER) {
            currentScreen = SCREEN_SETTINGS;
            return 0;
        }
    }

    if (kDown & KEY_SELECT) {
        settings.clockMode = (settings.clockMode + 1) % 3;
        settingsDirty = true;
    }

    if (kDown & KEY_DRIGHT) {
        settings.clockSizePreset = (settings.clockSizePreset + 1) % 4;
        settingsDirty = true;
    }
    if (kDown & KEY_DLEFT) {
        settings.clockSizePreset = (settings.clockSizePreset + 3) % 4;
        settingsDirty = true;
    }

    if (kDown & KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);

        if (alarmRinging) {
            stopAlarmRinging();
            return 0;
        }

        if (lowerScreenOff) {
            lowerScreenOff = false;
            return 0;
        }

        if (currentScreen == SCREEN_MAIN) {
            bool hitKnownArea = false;

            if (touch.px >= 15 && touch.px <= 105 && touch.py >= 185 && touch.py <= 217) {
                currentScreen = SCREEN_SETTINGS;
                hitKnownArea = true;
            }

            if (touch.px >= 115 && touch.px <= 205 && touch.py >= 185 && touch.py <= 217) {
                lowerScreenOff = true;
                hitKnownArea = true;
            }

            if (touch.px >= 215 && touch.px <= 305 && touch.py >= 185 && touch.py <= 217) {
                currentScreen = SCREEN_RECORDS;
                hitKnownArea = true;
            }

            if (!hitKnownArea) {
                if (!tryLaunchBottomJellyAt((float)touch.px, (float)touch.py)) {
                    setBottomJellyTarget((float)touch.px, (float)touch.py);
                }
            }
        }
        else if (currentScreen == SCREEN_SETTINGS) {
            if (touch.px >= 15 && touch.px <= 305) {
                if (touch.py >= 45 && touch.py <= 71) {
                    settings.clockColorIndex = (settings.clockColorIndex + 1) % 16;
                    settingsDirty = true;
                }

                if (touch.px <= 255 && touch.py >= 78 && touch.py <= 104) {
                    settings.bgThemeIndex = (settings.bgThemeIndex + 1) % 5;
                    settingsDirty = true;
                }
                if (touch.py >= 111 && touch.py <= 137) {
                    settings.timeFormat24h = !settings.timeFormat24h;
                    settingsDirty = true;
                }
                if (touch.py >= 144 && touch.py <= 170) {
                    settings.dateFormat = (settings.dateFormat + 1) % 3;
                    settingsDirty = true;
                }
            }

            if (touch.px >= 261 && touch.px <= 305 && touch.py >= 78 && touch.py <= 104) {
                settings.gradientInverted = !settings.gradientInverted;
                settingsDirty = true;
            }
            if (touch.px >= 15 && touch.px <= 84 && touch.py >= 190 && touch.py <= 220) {
                currentScreen = SCREEN_MAIN;
            }
            if (touch.px >= 89 && touch.px <= 158 && touch.py >= 190 && touch.py <= 220) {
                if (alarmCfg.enabled) {
                    alarmEditMinute = alarmCfg.minute;
                    if (!settings.timeFormat24h) {
                        u32 h12 = alarmCfg.hour % 12; if (h12 == 0) h12 = 12;
                        alarmEditHour = h12;
                        alarmEditPM   = (alarmCfg.hour >= 12);
                    } else {
                        alarmEditHour = alarmCfg.hour;
                        alarmEditPM   = false;
                    }
                } else {
                    alarmEditMinute = 0;
                    alarmEditPM     = false;
                    alarmEditHour   = settings.timeFormat24h ? 0 : 12;
                }
                currentScreen = SCREEN_ALARM;
            }
            if (touch.px >= 162 && touch.px <= 231 && touch.py >= 190 && touch.py <= 220) {
                if (timerState == TIMER_IDLE) {
                    timerEditHour   = timerTotalSeconds / 3600;
                    timerEditMinute = (timerTotalSeconds % 3600) / 60;
                    timerEditSecond = timerTotalSeconds % 60;
                }
                currentScreen = SCREEN_TIMER;
            }
            if (touch.px >= 236 && touch.px <= 305 && touch.py >= 190 && touch.py <= 220) {
                settings = (AppSettings){11, 1, 1, 0, 1, 0, 0, 0.0f, 0.0f};
                settingsDirty = true;
            }
        }
        else if (currentScreen == SCREEN_ALARM) {
            bool is12h = !settings.timeFormat24h;

            float hourFieldX, minFieldX, ampmFieldX;
            if (is12h) {
                const float gap = 10.0f;
                const float totalW = 80.0f + gap + 80.0f + gap + 66.0f;
                const float startX = 15.0f + (290.0f - totalW) * 0.5f;
                hourFieldX = startX;
                minFieldX  = hourFieldX + 80.0f + gap;
                ampmFieldX = minFieldX  + 80.0f + gap;
            } else {
                hourFieldX = 82.0f;
                minFieldX  = 172.0f;
                ampmFieldX = 0.0f;
            }

            if (touch.px >= hourFieldX && touch.px <= hourFieldX + 80 && touch.py >= 80 && touch.py <= 150) {
                if (is12h) alarmEditHour = (alarmEditHour % 12) + 1;
                else       alarmEditHour = (alarmEditHour + 1) % 24;
            }
            if (touch.px >= minFieldX && touch.px <= minFieldX + 80 && touch.py >= 80 && touch.py <= 150) {
                alarmEditMinute = (alarmEditMinute + 1) % 60;
            }
            if (is12h && touch.px >= ampmFieldX && touch.px <= ampmFieldX + 66 && touch.py >= 91 && touch.py <= 139) {
                alarmEditPM = !alarmEditPM;
            }
            if (touch.px >= 15 && touch.px <= 105 && touch.py >= 190 && touch.py <= 220) {
                currentScreen = SCREEN_SETTINGS;
            }
            if (touch.px >= 115 && touch.px <= 205 && touch.py >= 190 && touch.py <= 220) {
                alarmCfg.enabled = 1;
                if (is12h) {
                    u32 h24 = alarmEditHour % 12;
                    if (alarmEditPM) h24 += 12;
                    alarmCfg.hour = h24;
                } else {
                    alarmCfg.hour = alarmEditHour;
                }
                alarmCfg.minute  = alarmEditMinute;
                alarmLastTrigHour = -1;
                alarmLastTrigMin  = -1;
                alarmDirty = true;
            }
            if (touch.px >= 215 && touch.px <= 305 && touch.py >= 190 && touch.py <= 220) {
                alarmCfg = (AppAlarm){0, 0, 0};
                alarmDirty = true;
            }
        }
        else if (currentScreen == SCREEN_TIMER) {
            const float fieldW = 80.0f, gap = 10.0f;
            const float totalW = fieldW*3 + gap*2;
            const float startX = 15.0f + (290.0f - totalW) * 0.5f;
            float hourFieldX = startX;
            float minFieldX  = hourFieldX + fieldW + gap;
            float secFieldX  = minFieldX  + fieldW + gap;

            bool fieldsEditable = (timerState == TIMER_IDLE);

            if (fieldsEditable) {
                if (touch.px >= hourFieldX && touch.px <= hourFieldX + fieldW && touch.py >= 80 && touch.py <= 150) {
                    timerEditHour = (timerEditHour + 1) % 24;
                }
                if (touch.px >= minFieldX && touch.px <= minFieldX + fieldW && touch.py >= 80 && touch.py <= 150) {
                    timerEditMinute = (timerEditMinute + 1) % 60;
                }
                if (touch.px >= secFieldX && touch.px <= secFieldX + fieldW && touch.py >= 80 && touch.py <= 150) {
                    timerEditSecond = (timerEditSecond + 1) % 60;
                }
            }

            if (touch.px >= 15 && touch.px <= 105 && touch.py >= 190 && touch.py <= 220) {
                currentScreen = SCREEN_SETTINGS;
            }

            if (touch.px >= 115 && touch.px <= 205 && touch.py >= 190 && touch.py <= 220) {
                if (timerState == TIMER_IDLE) {
                    u32 totalSec = timerEditHour*3600 + timerEditMinute*60 + timerEditSecond;
                    if (totalSec > 0) {
                        timerTotalSeconds     = totalSec;
                        timerRemainingSeconds = totalSec;
                        timerFrameAccumulator = 0;
                        timerState = TIMER_RUNNING;
                    }
                } else if (timerState == TIMER_RUNNING) {
                    timerState = TIMER_PAUSED;
                } else {
                    timerFrameAccumulator = 0;
                    timerState = TIMER_RUNNING;
                }
            }

            if (touch.px >= 215 && touch.px <= 305 && touch.py >= 190 && touch.py <= 220) {
                if (timerState == TIMER_IDLE) {
                    timerEditHour = timerEditMinute = timerEditSecond = 0;
                } else {
                    timerState = TIMER_IDLE;
                    timerRemainingSeconds = 0;
                }
            }
        }
        else if (currentScreen == SCREEN_RECORDS) {

            if (touch.px >= 15 && touch.px <= 157 && touch.py >= 185 && touch.py <= 217) {
                currentScreen = SCREEN_CREDITS;
            }
            if (touch.px >= 163 && touch.px <= 305 && touch.py >= 185 && touch.py <= 217) {
                currentScreen = SCREEN_MAIN;
            }
        }
        else if (currentScreen == SCREEN_CREDITS) {
            if (touch.px >= 105 && touch.px <= 215 && touch.py >= 185 && touch.py <= 217)
                currentScreen = SCREEN_RECORDS;
        }
    }

    if (kDown & KEY_START) return 1;
    return 0;
}

int main() {
    gfxInitDefault();
    C3D_Init(0x10000);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top    = C2D_CreateScreenTarget(GFX_TOP,    GFX_LEFT);
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C2D_TextBuf topBuf    = C2D_TextBufNew(1024);
    C2D_TextBuf bottomBuf = C2D_TextBufNew(2048);

    mkdir(SAVE_DIR, 0777);

    loadRecords();
    loadSettings();
    loadAlarm();
    initOceanLife();

    {
        u32 jellyToRestore = records.savedJellyCount;
        if (jellyToRestore > BOTTOM_JELLY_MAX_COUNT) jellyToRestore = BOTTOM_JELLY_MAX_COUNT;
        for (u32 i = 0; i < jellyToRestore; i++) {
            spawnBottomJelly();
        }
    }
    initStaticTexts();
    initAudio();

    int prevHour = -1;
    int prevMin  = -1;
    int prevHalfHourSlot = -1;
    int prevTenMinSlot   = -1;
    int prevMinuteSlot   = -1;
    int prevThreeMinSlot = -1;
    time_t lastRawTime = 0;

    while (aptMainLoop()) {
        if (aptShouldJumpToHome()) {
            aptJumpToHomeMenu();
        }

        if (updateInput(topBuf)) break;

        sessionFrames++;

        if (sessionFrames >= 3600) {
            records.totalTimeSeconds += 60;
            sessionFrames -= 3600;
        }

        time_t raw = time(NULL);
        struct tm* tmv = localtime(&raw);

        bool wasSuspended = (lastRawTime != 0) && ((raw - lastRawTime) > 5);
        lastRawTime = raw;

        bool wasFirstFrame  = (prevHour == -1) || wasSuspended;
        bool hourJustChanged = (tmv->tm_hour != prevHour);
        if (hourJustChanged) {
            prevHour = tmv->tm_hour;
        }
        if (tmv->tm_min  != prevMin)  prevMin  = tmv->tm_min;

        int totalMinutes = tmv->tm_hour * 60 + tmv->tm_min;

        int minuteSlot = totalMinutes;
        bool wasFirstMinute = (prevMinuteSlot == -1) || wasSuspended;
        if (minuteSlot != prevMinuteSlot) {
            if (!wasFirstMinute && !anglerfish.active) {
                spawnAnglerfish();
            }
            prevMinuteSlot = minuteSlot;
        }

        int threeMinSlot = totalMinutes / 3;
        bool wasFirstThreeMin = (prevThreeMinSlot == -1) || wasSuspended;
        if (threeMinSlot != prevThreeMinSlot) {

            if (!wasFirstThreeMin && !sharkEvent.active && tmv->tm_min != 0) {
                spawnSharkEvent();
            }
            prevThreeMinSlot = threeMinSlot;
        }

        int quarterHourSlot = tmv->tm_hour * 4 + (tmv->tm_min / 15);
        bool wasFirstQuarterHour = (prevHalfHourSlot == -1) || wasSuspended;
        if (quarterHourSlot != prevHalfHourSlot) {
            if (!wasFirstQuarterHour && abyssEvent.phase != ABYSS_ACTIVE && tmv->tm_min != 0) {
                spawnAbyssEvent();
            }
            prevHalfHourSlot = quarterHourSlot;
        }

        int tenMinSlot = tmv->tm_hour * 6 + (tmv->tm_min / 10);
        bool wasFirstTenMin = (prevTenMinSlot == -1) || wasSuspended;
        if (tenMinSlot != prevTenMinSlot) {
            if (!wasFirstTenMin) {
                spawnOrFreeBottomJelly();
            }
            prevTenMinSlot = tenMinSlot;
        }

        int secondsToNextHour = (59 - tmv->tm_min) * 60 + (60 - tmv->tm_sec);
        if (hourEvent.phase == HOUR_EVENT_IDLE && secondsToNextHour <= 10 && !wasFirstFrame) {
            hourEvent.phase = HOUR_EVENT_CLEARING;
        }
        if (hourJustChanged && !wasFirstFrame) {

            int hourToShow = tmv->tm_hour;
            if (!settings.timeFormat24h) {
                hourToShow = tmv->tm_hour % 12;
                if (hourToShow == 0) hourToShow = 12;
            }
            buildHourSchoolFormation(hourToShow, false);
        }

        if (hourEvent.testClearingFramesLeft > 0) {
            hourEvent.testClearingFramesLeft--;
            if (hourEvent.testClearingFramesLeft == 0) {
                int hourToShow = tmv->tm_hour;
                if (!settings.timeFormat24h) {
                    hourToShow = tmv->tm_hour % 12;
                    if (hourToShow == 0) hourToShow = 12;
                }
                buildHourSchoolFormation(hourToShow, false);
            }
        }

        if (alarmCfg.enabled && !alarmRinging &&
            tmv->tm_hour == (int)alarmCfg.hour && tmv->tm_min == (int)alarmCfg.minute &&
            !(alarmLastTrigHour == tmv->tm_hour && alarmLastTrigMin == tmv->tm_min)) {
            startAlarmRinging();
            alarmLastTrigHour = tmv->tm_hour;
            alarmLastTrigMin  = tmv->tm_min;
        }

        updateAlarmRinging();
        updateTimer();

        globalAnimTime += 0.0166f;

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top);
        C2D_TextBufClear(topBuf);

        drawBackground();
        drawCaustics(globalAnimTime);

        updateAndDrawSharkEvent(globalAnimTime);
        updateAndDrawAbyssEvent(globalAnimTime);
        drawPlanktons(globalAnimTime);
        updateAndDrawBubbles(globalAnimTime);
        updateFishes(globalAnimTime);
        updateSchools(globalAnimTime);
        drawFishesBehindJellies(globalAnimTime);
        updateAndDrawSchools(globalAnimTime);

        processPendingJellyArrivals();
        updateAndDrawJellyDrifters(globalAnimTime);

        updateAndDrawScatterBubbles();
        drawJellyfishes(globalAnimTime);
        drawFishesInFrontOfJellies(globalAnimTime);

        updateAndDrawTurtleEvent(globalAnimTime);

        updateAndDrawHourSchoolFormation(globalAnimTime, true, true);

        if (settings.clockMode != 2) drawClock(topBuf, tmv);
        drawTimerCountdown(topBuf);
        updateAndDrawAnglerfish(globalAnimTime);

        C2D_TextBufClear(bottomBuf);
        drawBottomScreen(bottomBuf, bottom);

        C3D_FrameEnd(C3D_FRAME_SYNCDRAW);
    }

    if (settingsDirty) saveSettings();
    records.totalTimeSeconds += sessionFrames / 60;
    records.savedJellyCount = (u32)bottomJellyCount;
    saveRecords();
    if (alarmDirty) saveAlarm();

    exitAudio();

    C2D_TextBufDelete(topBuf);
    C2D_TextBufDelete(bottomBuf);
    C2D_TextBufDelete(staticBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();

    return 0;
}