// This file is part of: Geiss screensaver and Winamp music visualization plug-in
//                           ___ ___ ___ ___ ___
//                          / __| __|_ _/ __/ __|
//                         | (_ | _| | |\__ \__ \
//                          \___|___|___|___/___/
// 3-Clause BSD License
//
// Copyright (c) 1998-2022 Ryan Geiss (@geissomatik)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


// Native extraction: Win32 UI/DirectDraw excluded; LONG storage is fixed-width.
#include "engine_platform.hpp"
int g_nCores=1;
float AdjustRateToFPS(float per_frame_decay_rate_at_fps1, float fps1, float actual_fps)
{
    // returns the equivalent per-frame decay rate at actual_fps
    
    // basically, do all your testing at fps1 and get a good decay rate;
    // then, in the real application, adjust that rate by the actual fps each time you use it.
    
    float per_second_decay_rate_at_fps1 = powf(per_frame_decay_rate_at_fps1, fps1);
    float per_frame_decay_rate_at_fps2 = powf(per_second_decay_rate_at_fps1, 1.0f/actual_fps);

    return per_frame_decay_rate_at_fps2;
}
    char g_song_title[256];                // also used to display custom messages
    //#define g_song_tooltip_frames 20
    int g_song_tooltip_frames = 20;
    HFONT g_title_font = NULL;
    int g_song_title_y;
    int g_song_title_x;
    int g_title_R;// = 190 + rand() % 50;
    int g_title_G;// = 190 + rand() % 50;
    int g_title_B;// = 190 + rand() % 50;
    bool g_LastMessageWasCustom = false;

#if PLUGIN
    BOOL g_bDisableSongTitlePopups = false;  
    BOOL g_WinampMinimizedByHand = false;
    BOOL g_bMinimizeWinamp = false;//true;
    HCURSOR hOldCursor = NULL;
    WINDOWPLACEMENT wp_winamp;
    int g_random_songtitle_freq = 0;
#endif

    int last_frame_v = 0;   // for sound level-triggering
    int last_frame_slope = 0;

#if SAVER
    BOOL g_bAutostartCD = false;
    BOOL g_bDisableCdControls = false;
#endif


///int wave_brightness = 0;

#define NUM_FREQS 3
#define FREQ_SAMPLES 192
int16_t g_SoundBuffer[16384];

float g_phase_inc[NUM_FREQS];

float g_fSoundBuffer[16384];
LPGUID g_lpGuid;
BOOL SoundEnabled=TRUE;
BOOL SoundReady=FALSE;
BOOL SoundActive=FALSE;
BOOL SoundEmpty=FALSE;
int  oldSoundBufPos=0;
int  oldSoundBufNum=0;
//BOOL fOnWin95;
char szSoundDrivers[10][1024];
int iNumSoundDrivers = 0;
int iCurSoundDriver = 0;
int frames_since_silence = 0;
float avg_peaks = 100;
float current_vol = 0;
#define PAST_VOL_N 120
float past_vol[PAST_VOL_N];
int   past_vol_pos = 0;
float avg_vol = 1.0;
float avg_vol_wide = 1.0;
float avg_vol_narrow = 1.0;
uint32_t iVolumeSum = 1;
float debug_param = 1.0;
float damped_std_dev = 5;
bool bBeatMode = false;
bool bBigBeat = false;
float fBigBeatThreshold = 1.10;

float rand_array[2345];
float sqrt_tab[21][21];
int rand_array_pos = 0;

int iTrack = 0;      // current CD track
int iNumTracks = 0;  // # cd tracks
enum DISC_STATES { STOPPED, PLAYING, PAUSED, UNKNOWN, BUSY };
DISC_STATES eDiscState = UNKNOWN;
//BUSY means that Windows CD player is running & we can't control the cd.
//when BUSY is set, g_bDisableCdControls will also get set (for that execution)

// user volume control
int volpos = 10;                // from 0 to 20... 10 is middle.
float volscale = 0.20;//1.0;    // designed for Windows' line-in to be at 1/2 position.
int slider1 = 0;
int g_FramesSinceShift = 0;
bool g_bSlideShift = true;
int g_SlideShiftFreq = 33;  // 33% of the time
int g_SlideShiftMinFrames = 5; // min. no. of frames between shifts

// new shifting technique...
float g_ShiftMaxVol = 0;



#define FOURIER_DETAIL 24
float g_power[FOURIER_DETAIL];
float g_power_smoothed[FOURIER_DETAIL];




int g_hit = 0;
int AUTOMIN = 0;
//BOOL bScrollMap = false;
//BOOL new_bScrollMap = false;

float gF[6];
#define SCATTERVALS 226
float fScatter[SCATTERVALS];
int g_iScatterPos = 0;




    // globals for bkg map generation:
    float f1, f2, f3, f4;
    float old_f1, old_f2, old_f3, old_f4;
    //unsigned char p1, p2, p3, p4;
    float cx[10], cy[10], ci[10], cj[10], cr[10], cr_inv[10];
    int   ctype[10];
    int   R_offset, old_R_offset;
    int  y_map_pos = -1;
    float max_rad_inv;// = 1.0/sqrtf(FXW*FXW+FXH*FXH);
    float rmult;// = 640.0/(float)FXW;
    float rdiv;// = 1.0/rmult;
    float protective_factor;// = (FXW > 640) ? 640.0/(float)FXW : 1;

//----------------------------------------------------

#define num_vid_modes 11 //12

int  VidMode = -1;
BOOL VidModeAutoPicked = false;
int  iNumVidModes = 0;  // set by DDraw's enumeration
int  iDispBits=32;
//BOOL bModeX=false;
#if SAVER
    int32_t FXW = -1;         // to set default pick, go to EnumModesCallback()
    int32_t FXH = -1;         // to set default pick, go to EnumModesCallback()
    int  iVSizePercent = 100;//45;
#endif
#if PLUGIN
    int32_t FXW = -1;//320;   // to set default pick, go to EnumModesCallback()
    int32_t FXH = -1;//240;   // to set default pick, go to EnumModesCallback()
    int  iVSizePercent = 100;//35;//100;
#endif


typedef struct
{
    int lo_band;
    int hi_band;
    bool bFXPalette;
    int iFXPaletteNum;
    int c1, c2, c3;
} pal_td;
pal_td old_palette;


typedef struct
{
    int iMode;
    int iParam1;
    int iParam2;
    int iParam3;
    float fParam1;
    float fParam2;
    float fParam3;
    //int palType;      // 0 
} preset;
preset g_Preset[10];


typedef struct
{
    int  iDispBits;
    int  FXW;
    int  FXH;
    BOOL VIDEO_CARD_FUCKED;
    //BOOL bModeX;
    char name[64];
} VidModeInfo;
#define MAX_VID_MODES 512
VidModeInfo VidList[MAX_VID_MODES];

typedef struct 
{
  GUID guid;
  char szDesc[256];
  char szName[256];
} DDrawDeviceInfo;
#define MAX_DEVICES 16
DDrawDeviceInfo g_device[MAX_DEVICES];
int g_nDevices = 0;
GUID g_DDrawDeviceGUID;


enum modesel_state { NONE_SEL, MODE_SEL_1 };
enum preset_state  { NONE, LOAD_1, LOAD_2, SAVE_1, SAVE_2 };
modesel_state eModeSelState = { NONE_SEL };
preset_state  ePresetState = { NONE };
preset_state  eCustomMsgState = { NONE };
int iModeSelection = 0;
int iPresetNum     = 0;
int iCustomMsgNum  = 0;

//int  ThreadNr = 0;
BOOL g_rush_map = false;
BOOL g_QuitASAP = false;
BOOL g_GeissProcFinished = false;
BOOL g_bFirstRun = false;
BOOL g_bDumpFileCleared = false;
BOOL g_bDebugMode = false;
BOOL g_bSuppressHelpMsg = false;
BOOL g_bSuppressAllMsg = false;
BOOL g_DisclaimerAgreed = true;
BOOL g_ConfigAccepted = false;
BOOL g_Capturing = false;
BOOL g_bUseBeatDetection = true;
BOOL g_bSyncColorToSound = false;

bool g_bLoadPreset = false;
int  g_iPresetNum = -1;

/*-------------------------------------------------*/

//int                     iBands = 4;  // 0=always...5=never
int                     iRegistryDelay = 0;  // milliseconds per frame
bool                    bMMX;
//bool                    bCyrix;
bool                    bBypassAssembly = FALSE;
bool                    bExitOnMouse = FALSE;
bool                    bLocked = FALSE;
bool                    bPalLocked = FALSE;
//dither//BOOL                    bDither = TRUE;
//dither//BOOL                    bRandomDither = FALSE;
//dither//BOOL                    bMEGARandomDither = FALSE;
int waveform, new_waveform;
int last_mouse_x = -1;
int last_mouse_y = -1;

#define NUM_WAVES 6

//void RestoreCtrlAlt(void);
void  RestoreCtrlAlt(void);

void GetWaveData();
void RenderDots(unsigned char *VS1);
void RenderWave(unsigned char *VS1);

#define WAVE_5_BLEND_RANGE   50
const int MINBUFSIZE         = ((314+WAVE_5_BLEND_RANGE)*2 + 20);

int32_t     FX_YCUT             =90;               //skip this many vert. lines in calculation
int32_t     FX_YCUT_HIDE         =92; //= FX_YCUT + 2;
int32_t     FX_YCUT_NUM_LINES   =FXW*(FXH-FX_YCUT*2);
int32_t     FX_YCUT_xFXW_x8     =FX_YCUT*FXW*8;
int32_t     FX_YCUT_xFXW        =FX_YCUT*FXW;
int32_t     FX_YCUT_HIDE_xFXW     =FX_YCUT_HIDE*FXW;
int32_t     FXW_x_FXH           =FXW*FXH;
int32_t     BUFSIZE             =max(FXW*2, MINBUFSIZE);
    //int FX_YCUT_HIDE_xLINESIZE = FX_YCUT_HIDE*1280;


    int   modeprefs[128];    // values range from 0 to 5 stars; default is 3.
    int   modeprefs_total = -1;  // ---REMEMBER, RANGE is [1,NUM_MODES], INCLUSIVE!---

    float fps = 40.0f;
    float fps_at_last_mode_switch = 40.0f;
    //float fps_core = 30;
    float time_array[30];
    int   time_array_pos = 0;
    bool  time_array_ready = false;
    clock_t start_clock = 0;      //DEPRECATED
    clock_t clock_debt = 0;       //DEPRECATED
    clock_t core_clock_time = 0;  //DEPRECATED
    clock_t blit_clock_time = 0;  //DEPRECATED
    clock_t flip_clock_time = 0;  //DEPRECATED
    //clock_t temp_clock;//, start_clock = -1;
    char inifile[512];
    char szDEBUG[512];
    char szFPS[] = "abcdefghijkabcdefghijkabcdefghijkabcdefghijkabcdefghijk";
#if SAVER
    char szMCM[] = " [click mouse button to exit - press h for help] ";
#endif
#if PLUGIN
    char szMCM[] = " [press ESC to exit - press h for help] ";
#endif
    char szLM[] = " - screen is LOCKED -       ";
    char szULM[] = " - screen is unlocked -       ";
    char szMisc[512];
    char szTrack[] = " - stopped at track xxx/yyy -                                    ";
    
    char szH1[] = " q/e/u/g/d/a/y: control effects ";
#if SAVER
    //dither//char szH2[] = " i/o: toggle dither/toggle sound ";
    char szH2[] = " o: toggle sound ";
#endif
#if PLUGIN
    char szH2[] = " SPACE: show song title ";
#endif
    char szH3[] = " j/k: temporarily scale wave down/up ";
    char szH4[] = " L/T: (un)lock screen/palette,   < and >: rate screen ";
    char szH5[] = " n: new screen (random),   i: shifting on/off";
    char szH6[] = " w/p: change waveform/palette ";
    char szH7[] = " ##: pick map (01-25)";
    char szH8[] = " h/f: toggle help/fps displays ";
#if SAVER
    char szH9[] = " z x c v b:  << play pause stop >> (for CD) ";
#endif
#if PLUGIN
    char szH9[] = " z x c v b r s:  << play pause stop >> repeat shuffle (Winamp) ";
#endif
    //char szH10[]= " ESC/click: quit,        r: refresh screen edges ";
    char szH10[]= " ESC/click: quit ";
    char szCurrentCD[128];
    char szNewCD[128];

    int chaser_x[20], chaser_y[20], chaser_ptr;
    unsigned char chaser_r[20], chaser_g[20], chaser_b[20];

#if (GRFX==1)
    LPDIRECTDRAW            lpDD = NULL;           // DirectDraw object
    //LPDIRECTDRAW2           lpDD2 = NULL;          // DirectDraw object
    LPDIRECTDRAWSURFACE     lpDDSPrimary = NULL;   // DirectDraw primary surface
    LPDIRECTDRAWSURFACE     lpDDSBack = NULL;      // DirectDraw back surface
#endif
  bool g_bLost = false;
    LPDIRECTDRAWPALETTE        lpDDPal = NULL;
    PALETTEENTRY            ape[256];
    PALETTEENTRY            ape2[256];
    PALETTEENTRY            apetemp[256];
    int                     iBlendsLeftInPal;  // out of 64
    int                        iFramesPerPaletteTick = 7;

    //BOOL                    bActive;        // is application active?
    BOOL                    bUserQuit = FALSE;
    bool                    g_PassDialogReq = false;
    bool                    g_PassDialogAck = false;
    bool                    g_PowerSaveAck = false; // means the monitor shut off.
    bool                    g_PowerSaveReq = false;

    //void FX_Random_Palette();
    void FX_Random_Palette(bool bLoadPal = false);
    void PutPalette();
    float CrankPal(unsigned int curve_id, int z);



//#define DllImport   __declspec( dllimport )
//DllImport void Apply_DLL_Mode();






//typedef void (CALLBACK* LPFNDLLFUNC1)();
//typedef void (CALLBACK* LPFNDLLFUNC1)();

//HINSTANCE hMapLoaded;         // Handle to DLL

//typedef int (*MAPFUNCTION)(void);

//MAPFUNCTION mapfunction;

//LPFNDLLFUNC1 lpfnDllFunc1;    // Function pointer (to Apply_DLL_Mode())

//__declspec(dllimport) void DoMapFromDLL();



//extern "C" void __declspec(dllexport) WINAPI DoMapFromDLL();  






/*
#ifdef __cplusplus
extern "C" {
#endif
__declspec( dllimport ) void Apply_DLL_Mode();
#ifdef __cplusplus
}
#endif
*/

    unsigned char *original_VS[2];
    unsigned char *original_DATA_FX[2];

    unsigned char *VS1 = NULL;
    unsigned char *VS2 = NULL;
    unsigned char *TEMPPTR = NULL;
    unsigned char *DATA_FX = NULL;  //    each pixel has 6 bytes of info saved for it:
                                    //    d1,d2,d3,d4, and the offset to look at.
    unsigned char *DATA_FX2 = NULL;
    char winpath[512];
    
    float scale1, scale2, turn1, turn2;//, x_trans=0, y_trans=0;
    float old_scale1, old_scale2, old_turn1, old_turn2;
    float cos_turn1, cos_turn2, sin_turn1, sin_turn2;
    float center_dwindle, floatframe=0;
    int32_t  frames_this_mode=0, intframe=0, clearframes=4, chaser_offset;
    int32_t  last_mousemove_frame=-5;
    int   frames_til_auto_switch = 550; //...at 30 fps; but gets to be equiv. at the current fps.
    int   frames_til_auto_switch__registry = 550;
    int   geiss_gamma = 10;
    int   solar_pal_freq = 1;
    int   coarse_pal_freq = 1;
    int   frames_crunching_this_mode=0;
    int32_t  frames_since_last_plop=10;
    int       mode,     gXC,     gYC, solar_max, grid_dir;  // grid_dir is 1 or -1
    int   new_mode, new_gXC, new_gYC;
    float damping = 1.0f, new_damping = 1.0f;
    float suggested_damping = 1.0f;
    int   weightsum, weightsum_res_adjusted, old_weightsum;
    unsigned char _REMAP_VALUES[2048];
    unsigned char *REMAP, *REMAP2, *REMAP3;
    unsigned char VIDEO_CARD_FUCKED=0, RND=1;
    //unsigned char SHOW_TITLE=-1;
    unsigned char SHOW_DEBUG          = 0;
    unsigned char SHOW_MOUSECLICK_MSG = 0;
    unsigned char SHOW_LOCKED_MSG     = 0;
    unsigned char SHOW_UNLOCKED_MSG   = 0;
    unsigned char SHOW_TRACK_MSG      = 0;
    unsigned char SHOW_MODEPREFS_MSG  = 0;
    unsigned char SHOW_MISC_MSG       = 0;
    bool          SHOW_FPS            = false;
    bool          SHOW_HELP_MSG       = false;



    BOOL FX_Init();
    void FX_Fini();
    void FX_Pick_Random_Mode();  // sets global 'mode' to a valid random number.
    void FX_Apply_Mode(bool bLoadPreset = false, int iPresetNum = 0);         // sets up DATA_X and other misc. stuff based on 'mode'
    void Solid_Line(float frame, unsigned char far *VS2);
    void Two_Chasers(float frame);
    void Put_Helpmsg_To_Backbuffer();
    void Put_FPS_To_Backbuffer();
    void Put_Trackmsg_To_Backbuffer();
    void Put_Msg_To_Backbuffer(char *str);
    //void Put_Debugmsg_To_Backbuffer();
    //void Put_Lockedmsg_To_Backbuffer();
    //void Put_Unlockedmsg_To_Backbuffer();
    //void Put_Mouseclickmsg_To_Backbuffer();
    void Merge_All_VS_To_Backbuffer();
    //void Process_Map(void *p1, void *p2, LPDIRECTDRAWSURFACE lpDDSurf);
    //void Frame_Seq();
    void RenderFX();
    void Drop_Solar_Particles(int);
    void Drop_Solar_Particles_320(int);
    void CheckForNewCD();

    BOOL CALLBACK ConfigDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    BOOL CALLBACK Config2DialogProc(HWND  hwndDlg, UINT  uMsg, WPARAM  wParam, LPARAM  lParam);
    BOOL CALLBACK DisclaimerDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);
    BOOL CALLBACK AboutDialogProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

    BOOL doInit( HINSTANCE hInstance, int nCmdShow );
    void finiObjects( void );
    LRESULT CALLBACK WindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
    //int32_t FAR PASCAL WindowProc2( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

    
   extern HINSTANCE hMainInstance; /* screen saver instance handle  */ 

enum TScrMode {smNone,smConfig,smPassword,smPreview,smSaver};
TScrMode ScrMode=smNone;


LARGE_INTEGER m_high_perf_timer_freq;  // 0 if high-precision timer not available
LARGE_INTEGER m_prev_end_of_frame;
//int  g_nTargetFps = 9999;
//bool g_bTargetFpsStrict = false;
#define HALFSPEED_MOTION_CUTOFF_FRAMERATE ( 46.0f )

#define NUM_EFFECTS 9
#define NUM_MODES 25//16//19       // warning!  modes start at 1, not zero!

//int char_to_mode[256];
int effect[NUM_EFFECTS];
enum { CHASERS, BAR, DOTS, SOLAR, GRID, NUCLIDE, SHADE, SPECTRAL };  //'spectral' was the freq. spectrum dots; deprecated.

float micro_f1[10];
float micro_f2[10];
float micro_f3[10];
float micro_f4[10];
float micro_c1[10];
float micro_c2[10];
float micro_c3[10];
float micro_rad[4][10];

void FX_Screenshot(int fr);
void FX_Loadimage(char *s);
void GetWindowsPath(); // sets winpath
void ReadConfigRegistry();
void WriteConfigRegistry();
bool Try_To_Recover();
void dumpmsg(const char *format, ...);



bool mode_motion_dampened[] = 
{ 
    true, true, true, false, true, true, false, true, true, true,  // 0 is bunk; 1-9 here (3,6 not dampened)
    true, true, true, true, true, true, true,  // modes 10-16 are all dampened
    false, false, false,                       // 17 - 19 are normal
    false, false, false, false, false,         // 20 - 24 are normal
    false, false, false, false, false,         // 25 - 29 are normal
    false, false, false, false, false,         // 30 - 34 are normal
    false, false, false, false, false,         // 35 - 39 are normal
};

bool rotation_dither[] =
{ 
    false, true, false, false, false, false, false, false, false, true,   // 0-9 (0 is unused)
    false, true,  false, false, false,         // 10 - 14
    false, false, false, false, false,         // 15 - 19
    false, false, false, false, false,         // 20 - 24
    false, false, false, false, false,         // 25 - 29
    false, false, false, false, false,         // 30 - 34
    false, false, false, false, false,         // 35 - 39
};

bool custom_motion_vectors[] = 
{
    // 6, 10, 12 are true
    false, false, false, false, false, false, true, false, false, false,   // 0-9 (0 is unused)
    true,  false, true,  false, false,         // 10 - 14
    false, false, false, false, false,         // 15 - 19
    false, false, false, false, false,         // 20 - 24
    false, false, false, false, false,         // 25 - 29
    false, false, false, false, false,         // 30 - 34
    false, false, false, false, false,         // 35 - 39
};


class CModeInfo
{
public:

    CModeInfo();

    // all out of 1000.
    int effect_freq[NUM_EFFECTS];
    int solar_max;
    float center_dwindle;
    int max_effects;
    int min_effects;

    void Clip_Num_Effects();

protected:
    static int Get_Num_Effects();
};


CModeInfo::CModeInfo()
{
    min_effects = 1;
    max_effects = 2;
    solar_max = 60;
    center_dwindle = 0.99;
    for (int i=0; i<NUM_EFFECTS; i++) effect_freq[i] = 1000/NUM_EFFECTS;
}

int CModeInfo::Get_Num_Effects()
{
    int n = 0;
    for (int i=0; i<NUM_EFFECTS; i++)
        if (effect[i] > 0) 
            n++;
    
    return n;
}

void CModeInfo::Clip_Num_Effects()
{
    int n = Get_Num_Effects();
    int j;
    BOOL bGotOne;

    if (!SoundActive || SoundEmpty)
    while (n < min_effects)
    {
        bGotOne = FALSE;
        for (j=0; j<NUM_EFFECTS; j++)
            if ((effect[j]==-1) && (rand()%1000 < effect_freq[j]) && (bGotOne==FALSE))
            {
                effect[j] = 1;
                bGotOne = TRUE;
                n++;
            }
    }

    // make SURE the 1000-freq. ones get put in.
    for (j=0; j<NUM_EFFECTS; j++)
        if (effect_freq[j]>=1000)
            effect[j] = 1;

    while (n > max_effects)
    {
        j = rand() % NUM_EFFECTS;
        if (effect[j]==1 && effect_freq[j]<1000)
        {
            effect[j] = -1;
            n--;
        }
    }
}

CModeInfo modeInfo[NUM_MODES+1];
void FX_Random_Palette(bool); void PutPalette(); float CrankPal(unsigned int,int);

// This file is part of: Geiss screensaver and Winamp music visualization plug-in
//                           ___ ___ ___ ___ ___
//                          / __| __|_ _/ __/ __|
//                         | (_ | _| | |\__ \__ \
//                          \___|___|___|___/___/
// 3-Clause BSD License
//
// Copyright (c) 1998-2022 Ryan Geiss (@geissomatik)
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GEISS_EFFECTS
#define GEISS_EFFECTS 1


bool LoadCustomMsg(int iCustomMsgNum)
{
    char section[] = "CUSTOM MESSAGES";
	char message[64];
    sprintf(message, "MESSAGE %d", iCustomMsgNum);
		
	GetPrivateProfileString(section,message,"NOT FOUND",g_song_title,256,inifile);

	if (!strcmp(g_song_title, "NOT FOUND"))
	{
		dumpmsg("Custom message not found:");
		dumpmsg(section);
		dumpmsg(message);
		g_song_title[0] = 0;
		return false;
	}

	return true;

}

void LoadPreset(int n)
{
    char section[64];
    sprintf(section, "PRESET %d", n);

    char temp[64];
	float fTemp;

    GetPrivateProfileString(section,"x_center","-999",temp,64,inifile); sscanf(temp, "%f", &fTemp);
    if (fTemp < -998) 
    {
        dumpmsg("preset not found!");
        return; // preset not present
    }
    new_gXC = fTemp*FXW;
    GetPrivateProfileString(section,"y_center","0.5",temp,64,inifile); sscanf(temp, "%f", &fTemp);
	new_gYC = fTemp*FXH;

	new_mode     = GetPrivateProfileInt(section,"mode",5,inifile);
    new_waveform = GetPrivateProfileInt(section,"wave",1,inifile);
    GetPrivateProfileString(section,"damping","1.0",temp,64,inifile); sscanf(temp, "%f", &new_damping);
    weightsum = GetPrivateProfileInt(section,"magic",1,inifile);
	g_bSlideShift  = GetPrivateProfileInt(section,"shift",0,inifile);
	geiss_gamma = GetPrivateProfileInt(section,"geiss_gamma",0,inifile);
    
    int t = GetPrivateProfileInt(section,"volpos",10,inifile);
    while (volpos < t) { volpos++; volscale *= 1.25f; };
    while (volpos > t) { volpos--; volscale *= 0.80f; };


    // these 8 are handled internally by FX_Apply_Mode()  (init part)
    //char temp[64];
    //GetPrivateProfileString(section,"t1","1",temp,64,inifile); sscanf(temp, "%f", &turn1);
    //GetPrivateProfileString(section,"t2","1",temp,64,inifile); sscanf(temp, "%f", &turn2);
    //GetPrivateProfileString(section,"s1","1",temp,64,inifile); sscanf(temp, "%f", &scale1);
    //GetPrivateProfileString(section,"s2","1",temp,64,inifile); sscanf(temp, "%f", &scale2);
    //GetPrivateProfileString(section,"f1","1",temp,64,inifile); sscanf(temp, "%f", &f1);
    //GetPrivateProfileString(section,"f2","1",temp,64,inifile); sscanf(temp, "%f", &f2);
    //GetPrivateProfileString(section,"f3","1",temp,64,inifile); sscanf(temp, "%f", &f3);
    //GetPrivateProfileString(section,"f4","1",temp,64,inifile); sscanf(temp, "%f", &f4);
    
    // these 8 are handled internally by FX_Apply_Mode()  (final part)
    //effect[CHASERS] = GetPrivateProfileInt(section,"effect_chasers",0,inifile);
    //effect[BAR]     = GetPrivateProfileInt(section,"effect_bar"    ,0,inifile);
    //effect[DOTS]    = GetPrivateProfileInt(section,"effect_dots"   ,0,inifile);
    //effect[SOLAR]   = GetPrivateProfileInt(section,"effect_solar"  ,0,inifile);
    //effect[GRID]    = GetPrivateProfileInt(section,"effect_grid"   ,0,inifile);
    //effect[NUCLIDE] = GetPrivateProfileInt(section,"effect_nuclide",0,inifile);
    //effect[SHADE]   = GetPrivateProfileInt(section,"effect_shadebobs",0,inifile);

    old_palette.lo_band       = GetPrivateProfileInt(section,"pal_lo_band",-1,inifile);
    old_palette.hi_band       = GetPrivateProfileInt(section,"pal_hi_band",-1,inifile);
    old_palette.bFXPalette    = (bool)GetPrivateProfileInt(section,"pal_bFX",-1,inifile);
    old_palette.iFXPaletteNum = GetPrivateProfileInt(section,"pal_FXpalnum",-1,inifile);
    old_palette.c1            = GetPrivateProfileInt(section,"pal_curve_id_1",-1,inifile);
    old_palette.c2            = GetPrivateProfileInt(section,"pal_curve_id_2",-1,inifile);
    old_palette.c3            = GetPrivateProfileInt(section,"pal_curve_id_3",-1,inifile);

    #if PLUGIN
        visMode = (GetPrivateProfileInt(section,"spectrum",0,inifile) == 1) ? spectrum : wave;
    #endif
    
    FX_Random_Palette(true);
    y_map_pos = -1;
    g_rush_map = true;
    FX_Apply_Mode(true, n);

    
    

}

void SavePreset(int n)
{
    char section[64];
    sprintf(section, "PRESET %d", n);

    char string[64];

	sprintf(string,"%f",gXC/(float)FXW); WritePrivateProfileString(section,"x_center",string,inifile);
	sprintf(string,"%f",gYC/(float)FXH); WritePrivateProfileString(section,"y_center",string,inifile);
	sprintf(string,"%d",mode); WritePrivateProfileString(section,"mode",string,inifile);
	sprintf(string,"%f",damping); WritePrivateProfileString(section,"damping",string,inifile);
    sprintf(string,"%d",waveform); WritePrivateProfileString(section,"wave",string,inifile);
	sprintf(string,"%d",old_weightsum); WritePrivateProfileString(section,"magic",string,inifile);
	sprintf(string,"%d",volpos); WritePrivateProfileString(section,"volpos",string,inifile);
	sprintf(string,"%d",g_bSlideShift); WritePrivateProfileString(section,"shift",string,inifile);
	sprintf(string,"%d",geiss_gamma); WritePrivateProfileString(section,"geiss_gamma",string,inifile);
	sprintf(string,"%d",g_bSlideShift  ); WritePrivateProfileString(section,"shift",string,inifile);

	sprintf(string,"%f",old_turn1); WritePrivateProfileString(section,"t1",string,inifile);
	sprintf(string,"%f",old_turn2); WritePrivateProfileString(section,"t2",string,inifile);
	sprintf(string,"%f",old_scale1); WritePrivateProfileString(section,"s1",string,inifile);
	sprintf(string,"%f",old_scale2); WritePrivateProfileString(section,"s2",string,inifile);

	sprintf(string,"%f",old_f1); WritePrivateProfileString(section,"f1",string,inifile);
	sprintf(string,"%f",old_f2); WritePrivateProfileString(section,"f2",string,inifile);
	sprintf(string,"%f",old_f3); WritePrivateProfileString(section,"f3",string,inifile);
	sprintf(string,"%f",old_f4); WritePrivateProfileString(section,"f4",string,inifile);

	sprintf(string,"%d",effect[CHASERS]); WritePrivateProfileString(section,"effect_chasers",string,inifile);
	sprintf(string,"%d",effect[BAR]    ); WritePrivateProfileString(section,"effect_bar",string,inifile);
	sprintf(string,"%d",effect[DOTS]   ); WritePrivateProfileString(section,"effect_dots",string,inifile);
	sprintf(string,"%d",effect[SOLAR]  ); WritePrivateProfileString(section,"effect_solar",string,inifile);
	sprintf(string,"%d",effect[GRID]   ); WritePrivateProfileString(section,"effect_grid",string,inifile);
	sprintf(string,"%d",effect[NUCLIDE]); WritePrivateProfileString(section,"effect_nuclide",string,inifile);
	sprintf(string,"%d",effect[SHADE]  ); WritePrivateProfileString(section,"effect_shadebobs",string,inifile);
	sprintf(string,"%d",effect[SPECTRAL]);WritePrivateProfileString(section,"effect_spectral",string,inifile);

	sprintf(string,"%d",old_palette.lo_band      ); WritePrivateProfileString(section,"pal_lo_band",string,inifile);
	sprintf(string,"%d",old_palette.hi_band      ); WritePrivateProfileString(section,"pal_hi_oband",string,inifile);
	sprintf(string,"%d",(int)old_palette.bFXPalette); WritePrivateProfileString(section,"pal_bFX",string,inifile);
	sprintf(string,"%d",old_palette.iFXPaletteNum); WritePrivateProfileString(section,"pal_FXpalnum",string,inifile);
	sprintf(string,"%d",old_palette.c1           ); WritePrivateProfileString(section,"pal_curve_id_1",string,inifile);
	sprintf(string,"%d",old_palette.c2           ); WritePrivateProfileString(section,"pal_curve_id_2",string,inifile);
	sprintf(string,"%d",old_palette.c3           ); WritePrivateProfileString(section,"pal_curve_id_3",string,inifile);

    #if PLUGIN
        if (visMode==spectrum) WritePrivateProfileString(section,"spectrum","1",inifile);
        else                   WritePrivateProfileString(section,"spectrum","0",inifile);
    #else
        WritePrivateProfileString(section,"spectrum","0",inifile);
    #endif

}






void ShadeBobs()
{
	for (int x=0; x<effect[SHADE]; x++)
	{
		int cr,cg,cb;
		int k, a, b;
		int D_offset;

		cr = (int)(1 + sinf(floatframe*micro_c1[x]));
		cg = (int)(1 + sinf(floatframe*micro_c2[x]));
		cb = (int)(1 + sinf(floatframe*micro_c3[x]));

		a = gXC + (int)(micro_rad[0][x]*cosf(floatframe*micro_f1[x]) +  micro_rad[2][x]*cosf(floatframe*micro_f2[x]));  
		b = gYC + (int)(micro_rad[1][x]*cosf(floatframe*micro_f3[x]) +  micro_rad[3][x]*cosf(floatframe*micro_f4[x]));  
		for ( k=0; k<4; k++ )
		{
			b += (rand()%5) - 2;
			a += (rand()%5) - 2;
		//return;  // no problems if we return here
			if (b>FX_YCUT && b<FXH-1-FX_YCUT)
			{
//return; // screws up if we return here!
				D_offset = b*FXW + a;
				if (iDispBits==8)
				{
					if (VS1[D_offset    ] < 250) VS1[D_offset    ]+=2;
					if (VS1[D_offset+  1] < 250) VS1[D_offset+  1]+=1;
					if (VS1[D_offset-  1] < 250) VS1[D_offset-  1]+=1;
					if (VS1[D_offset+FXW] < 250) VS1[D_offset+FXW]+=1;
					if (VS1[D_offset-FXW] < 250) VS1[D_offset-FXW]+=1;
				}
				else //if (iDispBits==32)
				{
					D_offset *= 4;
					if (cr==1) 
					{
						if (VS1[D_offset    ] < 250) VS1[D_offset    ]+=5;
						if (VS1[D_offset+  4] < 250) VS1[D_offset+  4]+=3;
						if (VS1[D_offset-  4] < 250) VS1[D_offset-  4]+=3;
						if (VS1[D_offset+FXW*4] < 250) VS1[D_offset+FXW*4]+=3;
						if (VS1[D_offset-FXW*4] < 250) VS1[D_offset-FXW*4]+=3;
					}
					D_offset++;
					if (cg==1)
					{
						if (VS1[D_offset    ] < 250) VS1[D_offset    ]+=5;
						if (VS1[D_offset+  4] < 250) VS1[D_offset+  4]+=3;
						if (VS1[D_offset-  4] < 250) VS1[D_offset-  4]+=3;
						if (VS1[D_offset+FXW*4] < 250) VS1[D_offset+FXW*4]+=3;
						if (VS1[D_offset-FXW*4] < 250) VS1[D_offset-FXW*4]+=3;
					}
					D_offset++;
					if (cb==1)
					{
						if (VS1[D_offset    ] < 250) VS1[D_offset    ]+=5;
						if (VS1[D_offset+  4] < 250) VS1[D_offset+  4]+=3;
						if (VS1[D_offset-  4] < 250) VS1[D_offset-  4]+=3;
						if (VS1[D_offset+FXW*4] < 250) VS1[D_offset+FXW*4]+=3;
						if (VS1[D_offset-FXW*4] < 250) VS1[D_offset-FXW*4]+=3;
					}
				}
			}
		}
	}
}

void Diminish_Center(unsigned char *p1)
{
    bool bCenter = (mode != 12);    // center point, or vert line?

	if (center_dwindle < 0.999)
	{
        if (bCenter)
        {
            if (iDispBits>8)
            {
                for (int i=0; i<3; i++)
                {
		            if (p1[(gXC   +(gYC  )*FXW)*4] > 1) p1[(gXC   +(gYC  )*FXW)*4] = (unsigned char)(p1[(gXC   +(gYC  )*FXW)*4] * center_dwindle);
		            if (p1[(gXC-1 +(gYC  )*FXW)*4] > 1) p1[(gXC-1 +(gYC  )*FXW)*4] = (unsigned char)(p1[(gXC-1 +(gYC  )*FXW)*4] * center_dwindle);
		            if (p1[(gXC+1 +(gYC  )*FXW)*4] > 1) p1[(gXC+1 +(gYC  )*FXW)*4] = (unsigned char)(p1[(gXC+1 +(gYC  )*FXW)*4] * center_dwindle);
		            if (p1[(gXC   +(gYC+1)*FXW)*4] > 1) p1[(gXC   +(gYC+1)*FXW)*4] = (unsigned char)(p1[(gXC   +(gYC+1)*FXW)*4] * center_dwindle);
		            if (p1[(gXC   +(gYC-1)*FXW)*4] > 1) p1[(gXC   +(gYC-1)*FXW)*4] = (unsigned char)(p1[(gXC   +(gYC-1)*FXW)*4] * center_dwindle);
                    p1 = &p1[1];
                }
            }
            else
            {
		        if (p1[gXC   +(gYC  )*FXW] > 1) p1[gXC   +(gYC  )*FXW] = (unsigned char)(p1[gXC   +(gYC  )*FXW] * center_dwindle);
		        if (p1[gXC-1 +(gYC  )*FXW] > 1) p1[gXC-1 +(gYC  )*FXW] = (unsigned char)(p1[gXC-1 +(gYC  )*FXW] * center_dwindle);
		        if (p1[gXC+1 +(gYC  )*FXW] > 1) p1[gXC+1 +(gYC  )*FXW] = (unsigned char)(p1[gXC+1 +(gYC  )*FXW] * center_dwindle);
		        if (p1[gXC   +(gYC+1)*FXW] > 1) p1[gXC   +(gYC+1)*FXW] = (unsigned char)(p1[gXC   +(gYC+1)*FXW] * center_dwindle);
		        if (p1[gXC   +(gYC-1)*FXW] > 1) p1[gXC   +(gYC-1)*FXW] = (unsigned char)(p1[gXC   +(gYC-1)*FXW] * center_dwindle);
            }
        }
        else   // vertical line
        {
            int offset;
            if (iDispBits>8)
            {
                offset = (FX_YCUT*FXW + gXC) * 4;
                int o_inc = FXW*4;
                for (int i=FX_YCUT; i<FXH-FX_YCUT; i++)
                {
                    p1[offset-4] = (unsigned char)(p1[offset-4] * center_dwindle);
                    p1[offset-3] = (unsigned char)(p1[offset-3] * center_dwindle);
                    p1[offset-2] = (unsigned char)(p1[offset-2] * center_dwindle);
		            p1[offset  ] = (unsigned char)(p1[offset  ] * center_dwindle);
		            p1[offset+1] = (unsigned char)(p1[offset+1] * center_dwindle);
		            p1[offset+2] = (unsigned char)(p1[offset+2] * center_dwindle);
                    p1[offset+4] = (unsigned char)(p1[offset+4] * center_dwindle);
                    p1[offset+5] = (unsigned char)(p1[offset+5] * center_dwindle);
                    p1[offset+6] = (unsigned char)(p1[offset+6] * center_dwindle);
                    offset += o_inc;
                }
            }
            else
            {
                offset = (FX_YCUT*FXW + gXC);
                for (int i=FX_YCUT; i<FXH-FX_YCUT; i++)
                {
		            p1[offset-1] = (unsigned char)(p1[offset-1] * center_dwindle);
		            p1[offset  ] = (unsigned char)(p1[offset  ] * center_dwindle);
		            p1[offset+1] = (unsigned char)(p1[offset+1] * center_dwindle);
                    offset += FXW;
                }
            }
        }
	}
}


void Drop_Solar_Particles_320(int n)
{
	unsigned char p1, p2, p3;
	int x, y;
	int D_offset, k;
	float newx;
	int z, z0, z1, i0, i1, i2;
	int FXW4 = FXW * 4;


	if (iDispBits==8)
	{
		n *= 4;
	}

	for (k=n; k>0; k--)
	{
		newx = 100;
		while (newx >= 17)
		{
			y = rand() % 48 - 24;
			x = rand() % 64 - 32;
			newx = sqrtf(x*x + y*y);
		}
		y += gYC;
		x += gXC;
		if (y>FX_YCUT && y<FXH-1-FX_YCUT)
		{
			D_offset = y*320 + x;
		
			p1 = rand()%256;
			p2 = rand()%256;
			p3 = rand()%256;

			if (iDispBits==8)
			{
				i0 = ((17-newx)*12.0)*(p1/256.0);
				i1 = ((17-newx)* 3.0)*(p1/256.0);
				i2 = ((17-newx)* 0.8)*(p1/256.0);

				i0 = 2 + rand()%3;
				i1 = i0/2;
				i2 = i1/2;

				if (VS1[D_offset] < 207-i0) 
				{
						VS1[D_offset] += i0;
						VS1[D_offset+1] += i1;
						VS1[D_offset-1] += i1;
						VS1[D_offset+320] += i1;
						VS1[D_offset-320] += i1;
						VS1[D_offset-321] += i2;
						VS1[D_offset-319] += i2;
						VS1[D_offset+319] += i2;
						VS1[D_offset+321] += i2;
				}
			}
			else //if (iDispBits > 8)
			{
				D_offset *= 4;

				i0 = ((17-newx)*12.0)*(p1/256.0);
				i1 = ((17-newx)* 3.0)*(p1/256.0);
				i2 = ((17-newx)* 0.8)*(p1/256.0);
				if (VS1[D_offset] < 207-i0) 
				{
					VS1[D_offset] += i0;
					VS1[D_offset+4] += i1;
					VS1[D_offset-4] += i1;
					VS1[D_offset+FXW4] += i1;
					VS1[D_offset-FXW4] += i1;
					VS1[D_offset-FXW4-4] += i2;
					VS1[D_offset-FXW4+4] += i2;
					VS1[D_offset+FXW4-4] += i2;
					VS1[D_offset+FXW4+4] += i2;
				}
				D_offset ++;

				i0 = ((17-newx)*12.0)*(p2/256.0);
				i1 = ((17-newx)* 3.0)*(p2/256.0);
				i2 = ((17-newx)* 0.8)*(p2/256.0);
				if (VS1[D_offset] < 207-i0) 		
				{
						VS1[D_offset] += i0;
						VS1[D_offset+4] += i1;
						VS1[D_offset-4] += i1;
						VS1[D_offset+FXW4] += i1;
						VS1[D_offset-FXW4] += i1;
						VS1[D_offset-FXW4-4] += i2;
						VS1[D_offset-FXW4+4] += i2;
						VS1[D_offset+FXW4-4] += i2;
						VS1[D_offset+FXW4+4] += i2;
				}
				D_offset ++;

				i0 = ((17-newx)*12.0)*(p3/256.0);
				i1 = ((17-newx)* 3.0)*(p3/256.0);
				i2 = ((17-newx)* 0.8)*(p3/256.0);
				if (VS1[D_offset] < 207-i0)
				{
						VS1[D_offset] += i0;
						VS1[D_offset+4] += i1;
						VS1[D_offset-4] += i1;
						VS1[D_offset+FXW4] += i1;
						VS1[D_offset-FXW4] += i1;
						VS1[D_offset-FXW4-4] += i2;
						VS1[D_offset-FXW4+4] += i2;
						VS1[D_offset+FXW4-4] += i2;
						VS1[D_offset+FXW4+4] += i2;
				}
			}
		}
	}
}


void Drop_Solar_Particles(int n)
{
	int x, y;
	int D_offset, k;
	float newx, xf, yf;
	int z, z0, z1, i0, i1, i2;
	int FXW4 = FXW * 4;

	if (iDispBits==8)
	{
		n *= 4;
	}

	for (k=n; k>0; k--)
	{
		newx = 100;
		int its=0;
		while (newx >= 35)
		{
			y = (rand() % 72 - 36);
			x = (rand() % 96 - 48);
			newx = sqrtf(x*x + y*y);   // max. here is 80, but we clip at 48.
			//newx *= newx;
			//newx *= newx;
			//newx *= (1.0/(80.0*80.0));
			//its++;
			//if (its > 9999) exit(9999);
		}

		y += gYC;
		x += gXC;
		if (y>FX_YCUT && y<FXH-1-FX_YCUT)
		{
			D_offset = y*FXW + x;
		
			if (iDispBits==8)
			{
				i0 = 2 + rand()%2 + (35-newx)/9;
				i1 = i0-1;
				i2 = i1-1;
				if (VS1[D_offset] < 207-i0) 
				{
					VS1[D_offset] += i0;
					VS1[D_offset+1] += i1;
					VS1[D_offset-1] += i1;
					VS1[D_offset+FXW] += i1;
					VS1[D_offset-FXW] += i1;
					VS1[D_offset-FXW-1] += i2;
					VS1[D_offset-FXW+1] += i2;
					VS1[D_offset+FXW-1] += i2;
					VS1[D_offset+FXW+1] += i2;
				}
			}
			else //if (iDispBits > 8)
			{
				D_offset *= 4;

				i0 = 4 + rand()%30 * (35-newx)/25;
				i1 = i0-3;
				i2 = i1/2;
				if (VS1[D_offset] < 207-i0) 
				{
					VS1[D_offset] += i0;
					VS1[D_offset+4] += i1;
					VS1[D_offset-4] += i1;
					VS1[D_offset+FXW4] += i1;
					VS1[D_offset-FXW4] += i1;
					VS1[D_offset-FXW4-4] += i2;
					VS1[D_offset-FXW4+4] += i2;
					VS1[D_offset+FXW4-4] += i2;
					VS1[D_offset+FXW4+4] += i2;
				}
				D_offset ++;

				i0 = 4 + rand()%30 * (35-newx)/25;
				i1 = i0-3;
				i2 = i1/2;
				if (VS1[D_offset] < 207-i0) 		
				{
						VS1[D_offset] += i0;
						VS1[D_offset+4] += i1;
						VS1[D_offset-4] += i1;
						VS1[D_offset+FXW4] += i1;
						VS1[D_offset-FXW4] += i1;
						VS1[D_offset-FXW4-4] += i2;
						VS1[D_offset-FXW4+4] += i2;
						VS1[D_offset+FXW4-4] += i2;
						VS1[D_offset+FXW4+4] += i2;
				}
				D_offset ++;

				i0 = 4 + rand()%30 * (35-newx)/25;
				i1 = i0-3;
				i2 = i1/2;
				if (VS1[D_offset] < 207-i0)
				{
						VS1[D_offset] += i0;
						VS1[D_offset+4] += i1;
						VS1[D_offset-4] += i1;
						VS1[D_offset+FXW4] += i1;
						VS1[D_offset-FXW4] += i1;
						VS1[D_offset-FXW4-4] += i2;
						VS1[D_offset-FXW4+4] += i2;
						VS1[D_offset+FXW4-4] += i2;
						VS1[D_offset+FXW4+4] += i2;
				}
			}
		}
	}
}


void Solid_Line(float frame, unsigned char far *VS2)
{
	if (FXW==320)
	{
		int k, a, b, k2;
		float newx, newy, newx2, newy2;

		frame = frame*0.55/(0.08*20);
		newx = gXC + (12*cosf(frame*0.1102+10) +  15*cosf(frame*0.1312+20));  // DO NOT
		newy = gYC + (11*cosf(frame*0.1204+40) +  10*cosf(frame*0.1715+30));  // ERASE!
		newx2 = gXC + (10*cosf(frame*0.1213+33) +  13*cosf(frame*0.1408+15));  // DO NOT
		newy2 = gYC + ( 9*cosf(frame*0.1304+12) +  11*cosf(frame*0.1103+21));  // ERASE!

		for (k=0; k<43; k++)
		{
			a = newx*(k/43.0) + newx2*(1-k/43.0);
			b = newy*(k/43.0) + newy2*(1-k/43.0);
			if (b>FX_YCUT && b<FXH-1-FX_YCUT)
			{
				k2 = b*320+a;
				if (iDispBits>8) k2 *= 4;
				if (VS2[k2] < 223) VS2[k2] += 16;
			}
		}
	}
	else
	{
		int k, a, b, offset;
		float newx, newy, newx2, newy2;
		float s = FXW/640.0;

		frame = frame*0.55/(0.08*20);
		newx = gXC + (s*16*cosf(frame*0.1102+10) +  s*15*cosf(frame*0.1312+20));  // DO NOT
		newy = gYC + (s*15*cosf(frame*0.1204+40) +  s*10*cosf(frame*0.1715+30));  // ERASE!
		newx2 = gXC + (s*14*cosf(frame*0.1213+33) +  s*13*cosf(frame*0.1408+15));  // DO NOT
		newy2 = gYC + (s*13*cosf(frame*0.1304+12) +  s*11*cosf(frame*0.1103+21));  // ERASE!

		int k2 = s*50;
		float inv_s = 1.0f/(float)k2;
		for (k=0; k<k2; k++)
		{
			a = newx*(k*inv_s) + newx2*(1-k*inv_s);
			b = newy*(k*inv_s) + newy2*(1-k*inv_s);
			if (b>FX_YCUT && b<FXH-1-FX_YCUT)
			{
				offset = b*FXW+a;
				if (iDispBits>8) offset *= 4;
				if (VS2[offset] < 223) VS2[offset] += 16;
			}
		}
	}
}

void Two_Chasers(float frame)
{
	int k, a, b;

	float s = FXW/640.0;

	float t = frame;
	float time_scale = 1.0f;
	if (fps_at_last_mode_switch >= 10.0f && fps_at_last_mode_switch < 120.0f)
		time_scale = 30.0f/fps_at_last_mode_switch;
	t *= time_scale;

	// temp work: draw line
    /*
	{
		#include "draw.h"
		pt2d p, q;
		color_td color;
		color.r = 255;
		color.a = 255;//128 + 127*sinf(frame*0.07);
		p.x = gXC + (s*84*cosf(frame*0.1102+10) +  s*75*cosf(frame*0.1312+20));  
		p.y = gYC + (s*64*cosf(frame*0.1204+40) +  s*65*cosf(frame*0.1715+30)); 
		q.x = gXC + (s*54*cosf(frame*0.1213+33) +  s*65*cosf(frame*0.1408+15));  
		q.y = gYC + (s*62*cosf(frame*0.1304+12) +  s*61*cosf(frame*0.1103+21));  
		Smooth_Line(p, q, color, VS1, iDispBits, FXW);
	}
    */

	int passes = (effect[CHASERS]==2) ? 2 : 1;
	int k2 = (int)(20*s);
	for (k=0; k<k2; k++)
	{
		t = t + 0.08f*time_scale*20/(float)k2;
		for (int pass=0; pass<passes; pass++)
		{
			if (pass==0)
			{
				a = gXC + (int)(s*74*cosf(t*0.1102f+10.0f) +  s*65*cosf(t*0.1312f+20.0f));  
				b = gYC + (int)(s*54*cosf(t*0.1204f+40.0f) +  s*55*cosf(t*0.1715f+30.0f)); 
			}
			else
			{
				a = gXC + (int)(s*64*cosf(t*0.1213f+33.0f) +  s*55*cosf(t*0.1408f+15.0f));  
				b = gYC + (int)(s*52*cosf(t*0.1304f+12.0f) +  s*51*cosf(t*0.1103f+21.0f));  
			}

			if (b>FX_YCUT && b<FXH-1-FX_YCUT)
			{
				int offset = (b*FXW+a) * ((iDispBits==8) ? 1 : 4);
				if (iDispBits>8)
				{
					VS1[offset  ] = 255-(255-VS1[offset  ])*0.6f;
					VS1[offset+1] = 255-(255-VS1[offset+1])*0.6f;
					VS1[offset+2] = 255-(255-VS1[offset+2])*0.6f;
				}
				else //if (iDispBits==8)  //3.40
				{
					VS1[offset] = 255-(255-VS1[offset])*0.6f;
				}
			}
		}
	}
}


void Nuclide()
{
	// also see RenderWave()

	if (!SoundActive || SoundEmpty)// && iDispBits==8)
	{
		//if ((iDispBits==8 && rand()%28==0) || (iDispBits>8 && rand()%12==0))
		if (rand()%12==0)
		{
			int nodes = 3 + rand()%5;
			int n;
			int phase = rand()%1000;
			int rad = (20 + rand()%30);
			int x,y,cx,cy,str,val;
			int r;// = rand()%8 + 3;  // MAX is **10**

            if (FXW==320)
                r = 2 + rand()%6;
            else
                r = 3 + rand()%8;

			if (FXW==320)
				rad = 22 + rand()%6; //min(35, 14 + 0.7*vol);//(20 + rand()%30);
			else
				rad = 34 + rand()%8; //min(50, 20 + vol);//(20 + rand()%30);

			float cr=1.0, cg=1.0, cb=1.0;
			if (iDispBits>8)
			{
				int intfram = intframe + chaser_offset;
				float f = 7*sinf(intfram*0.007+29) + 5*cosf(intfram*0.0057+27);
				cr = 0.5 + 0.25*sinf(intfram*gF[0] +20-f) + 0.25*cosf(intfram*gF[3] +17+f);
				cg = 0.5 + 0.25*sinf(intfram*gF[1] +42+f) + 0.25*cosf(intfram*gF[4] +26-f);
				cb = 0.5 + 0.25*sinf(intfram*gF[2] +57-f) + 0.25*cosf(intfram*gF[5] +35+f);
			}

			for (n=0; n<nodes; n++)
			{
				cx = gXC + rad*cosf(n/(float)nodes * 6.28 + phase);
				cy = gYC + rad*sinf(n/(float)nodes * 6.28 + phase);
				if (cy-10>FX_YCUT && cy+10<FXH-1-FX_YCUT)
				{
					int32_t D_offset = cy*FXW + cx;
					if (iDispBits>8) D_offset *= 4; 
					int32_t T_offset;

					for (x=-10; x<10; x++)
					for (y=-10; y<10; y++)
					{
						T_offset = y*FXW + x;
						val = (r-sqrtf(x*x + y*y))*25;
						if (val>0)
						{
							if (iDispBits==8)
							{
								str = VS1[D_offset+T_offset] + val;
								VS1[D_offset+T_offset] = min(255, str);
							}
                            /*
							else if (iDispBits==16)
							{
								str = VS1[D_offset+T_offset] + val*cr;
								VS1[D_offset+T_offset] = min(255, str);
								str = VS3[D_offset+T_offset] + val*cg;
								VS3[D_offset+T_offset] = min(255, str);
								str = VS5[D_offset+T_offset] + val*cb;
								VS5[D_offset+T_offset] = min(255, str);
							}
                            */
							else //if (iDispBits==32)
							{
								T_offset *= 4;
								str = VS1[D_offset+T_offset] + val*cr;
								VS1[D_offset+T_offset] = min(255, str);
								str = VS1[D_offset+T_offset+1] + val*cg;
								VS1[D_offset+T_offset+1] = min(255, str);
								str = VS1[D_offset+T_offset+2] + val*cb;
								VS1[D_offset+T_offset+2] = min(255, str);
							}
						}
					}
				}
			}
		}
	}
}


/*
void Neutrons()
{
	if (iDispBits==8)
	{
		if (intframe%3==0 && rand()%3==0)
		{
			int x,y,cx,cy,str,val;
			cx = rand() % (FXW - 40) + 20;
			cy = rand() % (FXH - (FX_YCUT+20)*2 ) + (FX_YCUT+20);

			int32_t D_offset = cy*FXW + cx;
			int32_t T_offset;

			for (x=-10; x<10; x++)
			for (y=-10; y<10; y++)
			{
				T_offset = y*FXW + x;
				val = max(0, (10-sqrtf(x*x + y*y))*25);
    			str = VS1[D_offset+T_offset] + val;
				VS1[D_offset+T_offset] = min(255, str);
			}
		}
	}
}
*/

void One_Dotty_Chaser(float frame)
{
	int a, b, k;
	float s = FXW/640.0;

	float t = frame;
	if (fps_at_last_mode_switch >= 10.0f && fps_at_last_mode_switch < 120.0f)
		t *= 30.0f/fps_at_last_mode_switch;

	a = gXC + (int)(s*64*cosf(t*0.0613+33) +  s*55*cosf(t*0.0708+15));
	b = gYC + (int)(s*52*cosf(t*0.0704+12) +  s*51*cosf(t*0.0503+21));

	if (b>FX_YCUT && b<FXH-1-FX_YCUT)
	{
		chaser_ptr++;
		chaser_ptr %= 20;
		chaser_x[chaser_ptr] = a;
		chaser_y[chaser_ptr] = b;
		chaser_r[chaser_ptr] = 127+126*sinf(t*0.0613+33);
		chaser_g[chaser_ptr] = 127+126*sinf(t*0.0713+30);
		chaser_b[chaser_ptr] = 127+126*sinf(t*0.0513+27);

		for (k=0; k<20; k++)
		{
			if (iDispBits==8)
			{
				int offset = chaser_y[k]*FXW+chaser_x[k];
    			VS1[offset] = chaser_r[k];
				if (FXW >= 1050) //the palletized ones don't look good fat until later...
				{
    				VS1[offset+1] = chaser_r[k];
    				VS1[offset+FXW] = chaser_r[k];
    				VS1[offset+FXW+1] = chaser_r[k];
				}
			}
			else //if (iDispBits == 32)
			{
				int offset = (chaser_y[k]*FXW+chaser_x[k])*4;
    			VS1[offset  ] = chaser_r[k];
				VS1[offset+1] = chaser_g[k];
				VS1[offset+2] = chaser_b[k];
				if (FXW >= 880) //the truecolor ones look better FAT even at pretty low resolutions.
				{
    				VS1[offset+4] = chaser_r[k];
					VS1[offset+5] = chaser_g[k];
					VS1[offset+6] = chaser_b[k];
    				VS1[offset+FXW*4  ] = chaser_r[k];
					VS1[offset+FXW*4+1] = chaser_g[k];
					VS1[offset+FXW*4+2] = chaser_b[k];
    				VS1[offset+FXW*4+4] = chaser_r[k];
					VS1[offset+FXW*4+5] = chaser_g[k];
					VS1[offset+FXW*4+6] = chaser_b[k];
				}
			}
			chaser_x[k]++;
		}
	}
}



void Mode6Edges()
{
    int D_offset_1;
    int D_offset_2;
	int x;

    if (iDispBits>8)
    {
        D_offset_1 = (FXH-FX_YCUT-1)*FXW*4;
        D_offset_2 = (    FX_YCUT-1)*FXW*4;
        int c1,c2,c3,c4,c5,c6;

		for (x=0; x<FXW; x++)
		{
			c1 = 99 + 99*sinf(intframe*0.040 + x*0.031);    //red
			c2 = 99 + 99*sinf(intframe*-0.030 + x*0.024);
			c3 = 99 + 99*sinf(intframe*0.037 + x*0.035);    //green
			c4 = 99 + 99*sinf(intframe*-0.032 + x*0.028);
			c5 = 99 + 99*sinf(intframe*0.031 + x*0.033);    //blue
			c6 = 99 + 99*sinf(intframe*-0.035 + x*0.029);

			VS1[D_offset_1 + FXW*4] = rand()%40 + c1; 
			VS1[D_offset_1        ] = rand()%40 + c1; 
			VS1[D_offset_1 - FXW*4] = rand()%40 + c1; 
			VS1[D_offset_2 + FXW*4] = rand()%40 + c2; 
			VS1[D_offset_2        ] = rand()%40 + c2; 
			VS1[D_offset_2 - FXW*4] = rand()%40 + c2; 

            D_offset_1++;
            D_offset_2++;

			VS1[D_offset_1 + FXW*4] = rand()%40 + c3; 
			VS1[D_offset_1        ] = rand()%40 + c3; 
			VS1[D_offset_1 - FXW*4] = rand()%40 + c3; 
			VS1[D_offset_2 + FXW*4] = rand()%40 + c4; 
			VS1[D_offset_2        ] = rand()%40 + c4; 
			VS1[D_offset_2 - FXW*4] = rand()%40 + c4; 

            D_offset_1++;
            D_offset_2++;

			VS1[D_offset_1 + FXW*4] = rand()%40 + c5; 
			VS1[D_offset_1        ] = rand()%40 + c5; 
			VS1[D_offset_1 - FXW*4] = rand()%40 + c5; 
			VS1[D_offset_2 + FXW*4] = rand()%40 + c6; 
			VS1[D_offset_2        ] = rand()%40 + c6; 
			VS1[D_offset_2 - FXW*4] = rand()%40 + c6; 
            
            D_offset_1 += 2;
            D_offset_2 += 2;
		}
    }
    else
    {
        D_offset_1 = (FXH-FX_YCUT-1)*FXW;
        D_offset_2 = (    FX_YCUT-1)*FXW;
		for (x=0; x<FXW; x++)
		{
			int c1 = 40 + 40*sinf(intframe*0.040 + x*0.031);
			int c2 = 40 + 40*sinf(intframe*-0.030 + x*0.024);
			VS1[D_offset_1 + FXW] = rand()%40 + c1; 
			VS1[D_offset_1      ] = rand()%40 + c1; 
			VS1[D_offset_1 - FXW] = rand()%40 + c1; 
			VS1[D_offset_2 + FXW] = rand()%40 + c2; 
			VS1[D_offset_2      ] = rand()%40 + c2; 
			VS1[D_offset_2 - FXW] = rand()%40 + c2; 
            D_offset_1++;
            D_offset_2++;
		}
    
        D_offset_1 = (FXH-FX_YCUT-1)*FXW;
        D_offset_2 = (    FX_YCUT-1)*FXW;
        /*
		if (iDispBits == 16) 
            for (x=0; x<FXW; x++)
		    {
			    int c1 = 40 + 40*sinf(intframe*0.037 + x*0.035);
			    int c2 = 40 + 40*sinf(intframe*-0.032 + x*0.028);
			    VS3[D_offset_1 + FXW] = rand()%40 + c1; 
			    VS3[D_offset_1      ] = rand()%40 + c1; 
			    VS3[D_offset_1 - FXW] = rand()%40 + c1; 
			    VS3[D_offset_2 + FXW] = rand()%40 + c2; 
			    VS3[D_offset_2      ] = rand()%40 + c2; 
			    VS3[D_offset_2 - FXW] = rand()%40 + c2; 

			    c1 = 40 + 40*sinf(intframe*0.031 + x*0.033);
			    c2 = 40 + 40*sinf(intframe*-0.035 + x*0.029);
			    VS5[D_offset_1 + FXW] = rand()%40 + c1; 
			    VS5[D_offset_1      ] = rand()%40 + c1; 
			    VS5[D_offset_1 - FXW] = rand()%40 + c1; 
			    VS5[D_offset_2 + FXW] = rand()%40 + c2; 
			    VS5[D_offset_2      ] = rand()%40 + c2; 
			    VS5[D_offset_2 - FXW] = rand()%40 + c2; 

                D_offset_1++;
                D_offset_2++;
		    }
        */
    }
}


void Grid()
{
  int xInc = FXW/30;//(FXW >= 800) ? FXW/40 : FXW/30;//FXW/30;
	int yInc = xInc;//FXH/40;
    unsigned char s = max(0, 65 + 45*sinf(intframe*0.06033f*30.0f/fps_at_last_mode_switch  )  
                                + 35*cosf(intframe*0.04710f*30.0f/fps_at_last_mode_switch+1)
                                + 25*cosf(intframe*0.00523f*30.0f/fps_at_last_mode_switch-1)
                           );
	int x, dir, y;
	int k1, k;

	dir = (intframe%xInc);
	if (grid_dir==1)        // grid_dir is either 1 or -1
		dir *= -1;

  
	for (y=FX_YCUT_HIDE; y<FXH-FX_YCUT_HIDE; y+=yInc)
	{
		k1 = y*FXW;
		if (iDispBits>8) k1 *= 4;

		if (iDispBits==8)
		{
			if (FXW >= 1700)  // FAT PIXELS
			{
				for (x=0; x<FXW-1; x+=xInc)
				{
    				k = k1 + x + dir;
					if (VS1[k]<s) VS1[k] = s; //150;
					if (VS1[k+1]<s) VS1[k+1] = s; //150;
					if (VS1[k+FXW]<s) VS1[k+FXW] = s; //150;
					if (VS1[k+FXW+1]<s) VS1[k+FXW+1] = s; //150;
				}
			}
			else //skinny pixels
			{ 
				for (x=0; x<FXW; x+=xInc)
				{
    				k = k1 + x + dir;
					if (VS1[k]<s) VS1[k] = s; //150;
				}
			}
		}
		else //32-bit
		{
			if (FXW >= 1800)  // FAT PIXELS
			{
				for (x=0; x<FXW-1; x+=xInc)
				{
    				k = k1 + (x + dir)*4;
					if (VS1[k  ]<s) VS1[k  ] = s; //150;
					if (VS1[k+1]<s) VS1[k+1] = s; //150;
					if (VS1[k+2]<s) VS1[k+2] = s; //150;
					k += 4;
					if (VS1[k  ]<s) VS1[k  ] = s; //150;
					if (VS1[k+1]<s) VS1[k+1] = s; //150;
					if (VS1[k+2]<s) VS1[k+2] = s; //150;
					k += FXW*4-4;
					if (VS1[k  ]<s) VS1[k  ] = s; //150;
					if (VS1[k+1]<s) VS1[k+1] = s; //150;
					if (VS1[k+2]<s) VS1[k+2] = s; //150;
					k += 4;
					if (VS1[k  ]<s) VS1[k  ] = s; //150;
					if (VS1[k+1]<s) VS1[k+1] = s; //150;
					if (VS1[k+2]<s) VS1[k+2] = s; //150;
				}
			}
			else //skinny pixels
			{
				for (x=0; x<FXW-1; x+=xInc)
				{
    				k = k1 + (x + dir)*4;
					if (VS1[k  ]<s) VS1[k  ] = s; //150;
					if (VS1[k+1]<s) VS1[k+1] = s; //150;
					if (VS1[k+2]<s) VS1[k+2] = s; //150;
				}
			}
		}
	}
    

}
/*
void DoCrystals(int level, int xc, int yc)
{
    int spokes = 2 + rand()%(6);
    int length = 6 + rand()%(20);
    int phase = rand()%100;

    float t, dx, dy;
    int x, y, n, D_offset;

    for (n=0; n<spokes; n++)
    {
        dx = cosf(n/(float)spokes*6.28 + phase);
        dy = sinf(n/(float)spokes*6.28 + phase);
        for (t=0; t<length; t += 0.5)
        {
            x = xc + dx*t;
            y = yc + dy*t;
            D_offset = y*FXW + x;
            if (iDispBits==8)
            {
                if (VS1[D_offset] < 150) VS1[D_offset] = 150;
            }
            else
            {
                D_offset *= 4;
                if (VS1[D_offset  ] < 150) VS1[D_offset  ] = 150;
                if (VS1[D_offset+1] < 150) VS1[D_offset+1] = 150;
                if (VS1[D_offset+2] < 150) VS1[D_offset+2] = 150;
            }
        }
        //if (rand()%8==0)
        //{
        //    DoCrystals(level+1, x, y);
        //}
    }   
}
*/

#endif


float CrankPal(unsigned int curve_id, int z)
{
	float x = (float)z;

    
    /*
	if (SoundActive && !SoundEmpty && FXW==320)
	{
		x *= 0.8;
	}
    */

	switch(curve_id)
	{
	/*	
	case 1:	// good
		if (z>100) return 255; else return sqrtf(z)*25.4;
	case 2: // bright
		if (z<256/2) return z*2; else return 255;
	case 3: // BRIGHT ... better since 2-->1.5
		if (z<256/1.5) return z*1.5; else return 255;
	case 4:
		if (z<256/1.75) return z*1.75; else return 255;
	case 5:
		if (z>130) return 255; else return (z/130.0)*(z/130.0)*254;
		*/
	case 1:
		return sqrtf(x)*22.6;
	case 2:
		return x*2.0;
		//return 255-x;
	case 3:
		return x*x/64.0;
	case 4:
		return 255*sin(x/256.0*0.5*3.1415927);
	case 5:
    	return x*3.5;
    case 6:
		//return pow(2, x/24.0) - 1;
        return pow(1.5, x/20.0) - 1;    // this is really dark!
    case 7:
		//return 20.0*log(x);
    //case 8:
        return x*1.5 + 128*0.25 + 128*0.25*sin(z*0.3);
	}

	return 255.0;
}

void FX_Random_Palette(bool bLoadPal)
{
	if (bPalLocked) return;
	if (iDispBits!=8) return;

	int n, a, b;

    if (!bLoadPal)
    {
        old_palette.lo_band = -1;
        old_palette.hi_band = -1;
        old_palette.bFXPalette = false;
        old_palette.iFXPaletteNum = -1;
        old_palette.c1 = -1;
        old_palette.c2 = -1;
        old_palette.c3 = -1;

        if (rand()%10 < coarse_pal_freq)
        {
            old_palette.lo_band = 7 + rand()%6;
            old_palette.hi_band = 17 + rand()%6;
        }
    }

	iBlendsLeftInPal = 18;

    b = rand() % 6;
    if (bLoadPal)
	{
		if (old_palette.bFXPalette) b = 0; else b = 1;
	}

	if (b==0)    // generate monotone palette, from original FX.
	{
        old_palette.bFXPalette = true;
        if (!bLoadPal) old_palette.iFXPaletteNum = rand()%4;

		if (old_palette.iFXPaletteNum==0) 
		{ 
			for (a=0; a<128; a++) 
			{
				REMAP [a] = a*a/64.0;
				REMAP2[a] = a*2;
				REMAP3[a] = sqrtf(a)*22.6;
			} 
		} 
		else if (old_palette.iFXPaletteNum==1) 
		{ 
			for (a=0; a<128; a++) 
			{
				REMAP [a] = a*a/64.0;
				REMAP2[a] = sqrtf(a)*22.6;
				REMAP3[a] = a*2;
			}
		} 
		else if (old_palette.iFXPaletteNum==2) 
		{ 
			for (a=0; a<128; a++) 
			{
				REMAP [a] = sqrtf(a)*22.6;
				REMAP2[a] = a*2;
				REMAP3[a] = a*a/64.0;
			} 
		} 
		else if (old_palette.iFXPaletteNum==3) 
		{ 
			for (a=0; a<128; a++) 
			{
				REMAP [a] = a*2;
				REMAP2[a] = a*a/64.0;
				REMAP3[a] = sqrtf(a)*22.6;
			} 
		}

        /*
		for (a=0; a<128; a++) 
		{
			REMAP [a] = 63+63*sin(a*0.513);
			REMAP2[a] = 63+63*sin(a*0.536);
			REMAP3[a] = 63+63*sin(a*0.563);
		} 
        */


		for (n=128; n<256; n++)
		{
			REMAP [n] = REMAP [127];
			REMAP2[n] = REMAP2[127];
			REMAP3[n] = REMAP3[127];
		}

		for (n=0; n<256; n++) 
		{
			ape[n] = ape2[n];				// new
			ape2[n].peRed   = REMAP [n];	// switched to ape2
			ape2[n].peBlue  = REMAP2[n];
			ape2[n].peGreen = REMAP3[n];
		}

	}
	else   // generate palette from 3 random curves.
	{
        if (!bLoadPal)
        {
            int temp;

            do
            {
                if (rand()%5 < solar_pal_freq)
                {
			        old_palette.c1 = rand() % 7 + 1;
			        old_palette.c2 = rand() % 7 + 1;
			        old_palette.c3 = rand() % 7 + 1;
                }
                else
                {
			        old_palette.c1 = rand() % 6 + 1;
			        old_palette.c2 = rand() % 6 + 1;
			        old_palette.c3 = rand() % 6 + 1;
                }

                temp = 0; // measure of # of 'dark' color channels in palette
                if (old_palette.c1==6) temp++;
                if (old_palette.c2==6) temp++;
                if (old_palette.c3==6) temp++;
            }
            while (temp > 1);  // disallow really dark palettes.
        }
        
		float x,y,z;
        float gamma_factor = 1 + geiss_gamma*0.01;
        //float xs = 0.90 + 0.5*(rand() % 100)*0.01;
        //float ys = 0.90 + 0.5*(rand() % 100)*0.01;
        //float zs = 0.90 + 0.5*(rand() % 100)*0.01;
        //float mag = 1.0/sqrtf(xs*xs+ys*ys+zs*zs);
        //xs *= mag * 4;
        //ys *= mag;
        //zs *= mag;

        if (SoundEmpty) gamma_factor += 0.3;

		//int c = rand()%256;

		for (n=0; n<256; n++) 
		{
			ape[n] = ape2[n];
			
			x = CrankPal(old_palette.c1, (unsigned char)n);
			y = CrankPal(old_palette.c2, (unsigned char)n);
			z = CrankPal(old_palette.c3, (unsigned char)n);
            
			x *= gamma_factor;// * xs;
            y *= gamma_factor;// * ys;
            z *= gamma_factor;// * zs;

            if (n>old_palette.lo_band && n<old_palette.hi_band) 
            { 
				x *= 2; 
				y *= 2; 
				z *= 2; 
			}

			//float t = pow(2.718, -fabs(c-n)*0.1);
			//x = ape[n].peRed*t + x*(1-t);
			//y = ape[n].peBlue*t + y*(1-t);
			//z = ape[n].peGreen*t + z*(1-t);

			ape2[n].peRed   = (unsigned char)min(255.0, x);
			ape2[n].peBlue  = (unsigned char)min(255.0, y);
			ape2[n].peGreen = (unsigned char)min(255.0, z);
		}

	}

}

void PutPalette()
{
	float x, y;
	int n;

	if (iDispBits==8)
	{
		//if (--iBlendsLeftInPal == 0)
		//	FX_Random_Palette();
		
		iBlendsLeftInPal--;
        //dumpmsg("p");

		x = (iBlendsLeftInPal / 18.0);
		y = 1-x;

		for (n=0; n<256; n++)
		{
			apetemp[n].peRed =   ape[n].peRed*x + ape2[n].peRed*y;
			apetemp[n].peBlue =  ape[n].peBlue*x + ape2[n].peBlue*y;
			apetemp[n].peGreen = ape[n].peGreen*x + ape2[n].peGreen*y;
		}

		#if (GRFX==1)
			HRESULT ddrval = lpDDPal->SetEntries(0,0,256,apetemp);
			if (ddrval != DD_OK)
				iBlendsLeftInPal++;
			//lpDDPal->SetEntries(0,0,256,ape2);
		#endif
	}
}

void OnFxwFxhUpdated()
{
	//FX_YCUT could be zero, but that doesn't leave us with many black lines,
	// and our borders end up looking stupid...
	FX_YCUT             = max(4, (65 - 0.65*iVSizePercent)*0.5*0.01*FXH);//(65 - 0.60*iVSizePercent)*0.5*0.01*FXH
	FX_YCUT_HIDE		= FX_YCUT;// + 2;
	FX_YCUT_NUM_LINES   = FXW*(FXH-FX_YCUT*2);
	FX_YCUT_xFXW_x8     = FX_YCUT*FXW*8;
	FX_YCUT_xFXW        = FX_YCUT*FXW;
	FX_YCUT_HIDE_xFXW	= FX_YCUT_HIDE*FXW;
  FXW_x_FXH           = FXW*FXH;
  BUFSIZE             = max(FXW*2, MINBUFSIZE);
  #if PLUGIN
    BUFSIZE             = min(1023, BUFSIZE);   // because plugin only gives us 512 for each channel.
  #endif
}

BOOL FX_Init()
{
    uint32_t z, x;
    int d;

    dumpmsg("Starting FX_Init()");

    GetWindowsPath(); // sets winpath

    if (RND==1) srand( (unsigned)time( NULL ) );
    chaser_offset = rand() % 40000L;

    const size_t pixelBytes=size_t(FXW)*FXH*(iDispBits==8?1:4);
    const size_t mapBytes=size_t(FXW)*FXH*8;
    unsigned char* allocated[4]={
        (unsigned char*)calloc(1,pixelBytes+1024),
        (unsigned char*)calloc(1,pixelBytes+1024),
        (unsigned char*)calloc(1,mapBytes+1024),
        (unsigned char*)calloc(1,mapBytes+1024)};
    if(!allocated[0]||!allocated[1]||!allocated[2]||!allocated[3]) {
        for(int i=0;i<4;++i) free(allocated[i]);
        return FALSE;
    }
    original_VS[0]=VS1=allocated[0]; original_VS[1]=VS2=allocated[1];
    original_DATA_FX[0]=DATA_FX=allocated[2]; original_DATA_FX[1]=DATA_FX2=allocated[3];
    if (((uintptr_t)VS1) % 16 != 0)
    {
        dumpmsg("align VS1");
        VS1 = (unsigned char *)((((uintptr_t)VS1)/16 + 1) * 16);
    }
    if (((uintptr_t)VS2) % 16 != 0)
    {
        dumpmsg("align VS2");
        VS2 = (unsigned char *)((((uintptr_t)VS2)/16 + 1) * 16);
    }
    if (((uintptr_t)DATA_FX) % 16 != 0)
    {
        dumpmsg("align DATA_FX");
        DATA_FX = (unsigned char *)((((uintptr_t)DATA_FX)/16 + 1) * 16);
    }
    if (((uintptr_t)DATA_FX2) % 16 != 0)
    {
        dumpmsg("align DATA_FX2");
        DATA_FX2 = (unsigned char *)((((uintptr_t)DATA_FX2)/16 + 1) * 16);
    }



    memset(g_power,          0, sizeof(float)*FOURIER_DETAIL);
    memset(g_power_smoothed, 0, sizeof(float)*FOURIER_DETAIL);


//#if PLUGIN
//#endif

    //float s;// = 0.1*((rand()%100)*0.01);      // 0.0: regular palette, 0.4: can get wild palettes
    //s = 0.1;

    if (iDispBits==16)
    {
        for (z=0; z<256; z++) REMAP[z] = min(255, z*2) >> 3;
        for (z=0; z<256; z++) REMAP2[z] = min(255, z*2) >> 2;
    }
    else
    {
        for (z=0; z<256; z++) REMAP[z] = min(255, z*2);
    }
    
    for (z=0; z<SCATTERVALS; z++)
    {
        fScatter[z] = 0.05 - 0.025*((rand()%1000)*0.001);
    }

    frames_since_last_plop = 1;

    for (z=0; z<20; z++)
    {
        chaser_x[z] = 1;
        chaser_y[z] = 1;
    }

    for (x=0; x<=20; x++)
    for (z=0; z<=20; z++)
        sqrt_tab[x][z] = (float)sqrtf((x-10)*(x-10) + (z-10)*(z-10));

    for (z=0; z<10; z++)
    {
        micro_c1[z] = 0.08 + 0.09*(rand()%1000)*0.001;
        micro_c2[z] = 0.08 + 0.09*(rand()%1000)*0.001;
        micro_c3[z] = 0.08 + 0.09*(rand()%1000)*0.001;
        micro_f1[z] = 0.1 + 0.05*(rand()%1000)*0.001;
        micro_f2[z] = 0.1 + 0.05*(rand()%1000)*0.001;
        micro_f3[z] = 0.1 + 0.05*(rand()%1000)*0.001;
        micro_f4[z] = 0.1 + 0.05*(rand()%1000)*0.001;
        micro_rad[0][z] = 2.0 + 2.8*(rand()%1000)*0.001;
        micro_rad[1][z] = 2.0 + 2.8*(rand()%1000)*0.001;
        micro_rad[2][z] = 2.0 + 2.8*(rand()%1000)*0.001;
        micro_rad[3][z] = 2.0 + 2.8*(rand()%1000)*0.001;
    }

    for (z=0; z<6; z++) gF[z] = ((rand()%1000)*0.001)*0.01 + 0.02;

    for (z=0; z<2345; z++) rand_array[z] = (rand() % 100)*0.0005;  // for mode #7

    /*
    for (z=0; z<256; z++) char_to_mode[z] = 5;

    //g_font.load_font("c:\\courier_8bit.tga");

    if (NUM_MODES >= 1) char_to_mode['1'] = 1;
    if (NUM_MODES >= 2) char_to_mode['2'] = 2;
    if (NUM_MODES >= 3) char_to_mode['3'] = 3;
    if (NUM_MODES >= 4) char_to_mode['4'] = 4;
    if (NUM_MODES >= 5) char_to_mode['5'] = 5;
    if (NUM_MODES >= 6) char_to_mode['6'] = 6;
    if (NUM_MODES >= 7) char_to_mode['7'] = 7;
    if (NUM_MODES >= 8) char_to_mode['8'] = 8;
    if (NUM_MODES >= 9) char_to_mode['9'] = 9;
    if (NUM_MODES >= 10) char_to_mode['0'] = 10;
    if (NUM_MODES >= 11) char_to_mode['!'] = 11;
    if (NUM_MODES >= 12) char_to_mode['@'] = 12;
    if (NUM_MODES >= 13) char_to_mode['#'] = 13;
    if (NUM_MODES >= 14) char_to_mode['$'] = 14;
    if (NUM_MODES >= 15) char_to_mode['%'] = 15;
    if (NUM_MODES >= 16) char_to_mode['^'] = 16;
    if (NUM_MODES >= 17) char_to_mode['&'] = 17;
    if (NUM_MODES >= 18) char_to_mode['*'] = 18;
    if (NUM_MODES >= 19) char_to_mode['('] = 19;
    if (NUM_MODES >= 20) char_to_mode[')'] = 20;
    */
    
    z = 1;
    modeInfo[z].effect_freq[CHASERS] = 220;
    modeInfo[z].effect_freq[BAR    ] = 150;
    modeInfo[z].effect_freq[DOTS   ] =  10;
    modeInfo[z].effect_freq[SOLAR  ] = 680;
    modeInfo[z].effect_freq[GRID   ] =   4;
    modeInfo[z].effect_freq[NUCLIDE] = 170;
    modeInfo[z].effect_freq[SHADE  ] = 400;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = (iDispBits==8) ? 400 : 800;
    modeInfo[z].center_dwindle = 1.0;

    z = 2;
    modeInfo[z].effect_freq[CHASERS] = 750;
    modeInfo[z].effect_freq[BAR    ] = 500;
    modeInfo[z].effect_freq[DOTS   ] = 750;
    modeInfo[z].effect_freq[SOLAR  ] = 750;
    modeInfo[z].effect_freq[GRID   ] = 0;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] = 0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 35;
    modeInfo[z].center_dwindle = 1.0;
    modeInfo[z].max_effects = 5;

    z = 3;
    modeInfo[z].effect_freq[CHASERS] = 100;
    modeInfo[z].effect_freq[BAR    ] = 100;
    modeInfo[z].effect_freq[DOTS   ] = 100;
    modeInfo[z].effect_freq[SOLAR  ] = 500;
    modeInfo[z].effect_freq[GRID   ] = 10;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] = 300;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 60;
    modeInfo[z].center_dwindle = 0.99;

    z = 4;
    modeInfo[z].effect_freq[CHASERS] = 500;
    modeInfo[z].effect_freq[BAR    ] = 100;
    modeInfo[z].effect_freq[DOTS   ] = 100;
    modeInfo[z].effect_freq[SOLAR  ] = 100;
    modeInfo[z].effect_freq[GRID   ] = 30;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] = 0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 34;
    modeInfo[z].center_dwindle = 0.98;

    z = 5;
    modeInfo[z].effect_freq[CHASERS] = 100;
    modeInfo[z].effect_freq[BAR    ] = 350;
    modeInfo[z].effect_freq[DOTS   ] = 100;
    modeInfo[z].effect_freq[SOLAR  ] = 500;
    modeInfo[z].effect_freq[GRID   ] =  15;
    modeInfo[z].effect_freq[NUCLIDE] = 180;
    modeInfo[z].effect_freq[SHADE  ] = 500;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 60;
    modeInfo[z].center_dwindle = 0.99;

    z = 6;
    modeInfo[z].effect_freq[CHASERS] = 400;
    modeInfo[z].effect_freq[BAR    ] = 120;
    modeInfo[z].effect_freq[DOTS   ] = 200;
    modeInfo[z].effect_freq[SOLAR  ] =   0;
    modeInfo[z].effect_freq[GRID   ] =   0;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] =   0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 60;
    modeInfo[z].center_dwindle = 1.000;
    //modeInfo[z].min_effects = 0;   // we allow 0 effects because we still have the t/b edge effects.

    z = 7;
    modeInfo[z].effect_freq[CHASERS] =  50;
    modeInfo[z].effect_freq[BAR    ] = 200;
    modeInfo[z].effect_freq[DOTS   ] =   0;
    modeInfo[z].effect_freq[SOLAR  ] = 300;
    modeInfo[z].effect_freq[GRID   ] =   0;
    modeInfo[z].effect_freq[NUCLIDE] = 600;
    modeInfo[z].effect_freq[SHADE  ] = 350;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 65;
    modeInfo[z].center_dwindle = 0.985;

    z = 8;
    modeInfo[z].effect_freq[CHASERS] = 150;
    modeInfo[z].effect_freq[BAR    ] = 150;
    modeInfo[z].effect_freq[DOTS   ] = 150;
    modeInfo[z].effect_freq[SOLAR  ] = 150;
    modeInfo[z].effect_freq[GRID   ] =  25;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] = 
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 60;
    modeInfo[z].center_dwindle = 0.96;

    z = 9;
    modeInfo[z].effect_freq[CHASERS] = 450;
    modeInfo[z].effect_freq[BAR    ] = 200;
    modeInfo[z].effect_freq[DOTS   ] =  50;
    modeInfo[z].effect_freq[SOLAR  ] = 200;
    modeInfo[z].effect_freq[GRID   ] =   0;
    modeInfo[z].effect_freq[NUCLIDE] = 100;
    modeInfo[z].effect_freq[SHADE  ] = 200;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 50;
    modeInfo[z].center_dwindle = 0.985;

    z = 10;
    modeInfo[z].effect_freq[CHASERS] = 150;
    modeInfo[z].effect_freq[BAR    ] =  20;
    modeInfo[z].effect_freq[DOTS   ] =  80;
    modeInfo[z].effect_freq[SOLAR  ] =   0;
    modeInfo[z].effect_freq[GRID   ] =   0;
    modeInfo[z].effect_freq[NUCLIDE] =  80;
    modeInfo[z].effect_freq[SHADE  ] =   0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 0;
    modeInfo[z].center_dwindle = 1.0;   // no dwindle
    modeInfo[z].min_effects = 0;   
    modeInfo[z].max_effects = 2;   

    z = 11;
    modeInfo[z].effect_freq[CHASERS] = 360;
    modeInfo[z].effect_freq[BAR    ] = 200;
    modeInfo[z].effect_freq[DOTS   ] = 230;
    modeInfo[z].effect_freq[SOLAR  ] = 550;
    modeInfo[z].effect_freq[GRID   ] =  10;
    modeInfo[z].effect_freq[NUCLIDE] = 330;
    modeInfo[z].effect_freq[SHADE  ] = 150;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 750;
    modeInfo[z].center_dwindle = 1.0;   // no dwindle
    modeInfo[z].min_effects = 0;   
    modeInfo[z].max_effects = 4;   

    z = 12;  // sideways splitter
    modeInfo[z].effect_freq[CHASERS] = 360;
    modeInfo[z].effect_freq[BAR    ] = 200;
    modeInfo[z].effect_freq[DOTS   ] = 230;
    modeInfo[z].effect_freq[SOLAR  ] =   0;
    modeInfo[z].effect_freq[GRID   ] =   0;
    modeInfo[z].effect_freq[NUCLIDE] = 330;
    modeInfo[z].effect_freq[SHADE  ] =   0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 500;
    modeInfo[z].center_dwindle = 0.915;   // no dwindle
    modeInfo[z].min_effects = 0;   
    modeInfo[z].max_effects = 2;   

    z = 13;
    modeInfo[z].effect_freq[CHASERS] = 500;
    modeInfo[z].effect_freq[BAR    ] =   0;
    modeInfo[z].effect_freq[DOTS   ] = 100;
    modeInfo[z].effect_freq[SOLAR  ] =   0;
    modeInfo[z].effect_freq[GRID   ] = 30;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] = 0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 34;
    modeInfo[z].center_dwindle = 0.98;

    z = 14;
    modeInfo[z].effect_freq[CHASERS] = 500;
    modeInfo[z].effect_freq[BAR    ] =   0;
    modeInfo[z].effect_freq[DOTS   ] = 100;
    modeInfo[z].effect_freq[SOLAR  ] =   0;
    modeInfo[z].effect_freq[GRID   ] = 30;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] = 0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 34;
    modeInfo[z].center_dwindle = 0.98;

    z = 15;
    modeInfo[z].effect_freq[CHASERS] =   0;
    modeInfo[z].effect_freq[BAR    ] =   0;
    modeInfo[z].effect_freq[DOTS   ] =   0;
    modeInfo[z].effect_freq[SOLAR  ] =   0;
    modeInfo[z].effect_freq[GRID   ] =   0;
    modeInfo[z].effect_freq[NUCLIDE] = 200;
    modeInfo[z].effect_freq[SHADE  ] =   0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].center_dwindle = 1.00;
    modeInfo[z].min_effects = 0;
    modeInfo[z].max_effects = 1;

    z = 16;
    modeInfo[z].effect_freq[CHASERS] = 500;
    modeInfo[z].effect_freq[BAR    ] = 100;
    modeInfo[z].effect_freq[DOTS   ] = 100;
    modeInfo[z].effect_freq[SOLAR  ] = 100;
    modeInfo[z].effect_freq[GRID   ] = 30;
    modeInfo[z].effect_freq[NUCLIDE] =   0;
    modeInfo[z].effect_freq[SHADE  ] =   0;
    modeInfo[z].effect_freq[SPECTRAL]=   0;
    modeInfo[z].solar_max = 34;
    modeInfo[z].center_dwindle = 0.98;

    for (z=17; z<=NUM_MODES; z++)
    {
        modeInfo[z].effect_freq[CHASERS] = 150;
        modeInfo[z].effect_freq[BAR    ] = 150;
        modeInfo[z].effect_freq[DOTS   ] = 150;
        modeInfo[z].effect_freq[SOLAR  ] = 150;
        modeInfo[z].effect_freq[GRID   ] =  12;
        modeInfo[z].effect_freq[NUCLIDE] =   0;
        modeInfo[z].effect_freq[SHADE  ] =  50;
        modeInfo[z].effect_freq[SPECTRAL]=   0;
        modeInfo[z].solar_max = 600;
        modeInfo[z].min_effects = 1;
        modeInfo[z].max_effects = 3;
        modeInfo[z].center_dwindle = 1.000;
    }

    for (z=1; z<=NUM_MODES; z++) {
      if (iDispBits > 8) {
          modeInfo[z].effect_freq[NUCLIDE] = max(0, min(900, modeInfo[z].effect_freq[NUCLIDE]*1.3f));
          modeInfo[z].effect_freq[CHASERS] = max(0, min(900, modeInfo[z].effect_freq[CHASERS]- 50));
          modeInfo[z].effect_freq[DOTS]    = max(0, min(900, modeInfo[z].effect_freq[DOTS   ]+220));
          modeInfo[z].effect_freq[BAR ]    = max(0, min(900, modeInfo[z].effect_freq[BAR    ]+220));
          modeInfo[z].effect_freq[SHADE]   = max(0, min(900, modeInfo[z].effect_freq[SHADE  ]+150));
          //modeInfo[z].max_effects          += 1;
      }
        modeInfo[z].effect_freq[GRID]    = min(1000, modeInfo[z].effect_freq[GRID]+8);
    }

    // custom settings for modes 16+ :
    modeInfo[20].center_dwindle = 0.98;
    modeInfo[21].center_dwindle = 0.98; // fragmented (integer move)
    modeInfo[22].center_dwindle = 0.98; // sonic rings #1
    modeInfo[23].center_dwindle = 0.98; // sonic rings #2
    


    
    dumpmsg("  Calling initial FX_Pick_Random_Mode() and FX_Apply_Mode() loop...");

    //this is hanging:
    FX_Pick_Random_Mode();   // sets global 'mode' to a valid random number.
    g_rush_map = true;
    y_map_pos = -1;
    do
    {
        FX_Apply_Mode();         // sets up DATA_X and other misc. stuff based on 'mode'
    }
    while (y_map_pos != -1);

    dumpmsg("Finished with FX_Init().");

    return true;
}

void FX_Fini()
{
    if (original_VS[0] != NULL)      { free(original_VS[0]);      original_VS[0] = NULL;      VS1 = NULL; }
    if (original_VS[1] != NULL)      { free(original_VS[1]);      original_VS[1] = NULL;      VS2 = NULL; }
    if (original_DATA_FX[0] != NULL) { free(original_DATA_FX[0]); original_DATA_FX[0] = NULL; DATA_FX = NULL; }
    if (original_DATA_FX[1] != NULL) { free(original_DATA_FX[1]); original_DATA_FX[1] = NULL; DATA_FX2 = NULL; }

#if PLUGIN
    if (g_title_font) { DeleteObject(g_title_font); g_title_font = NULL; }
#endif

}

void FX_Pick_Random_Mode()
{
    if (modeprefs_total <= 0)
    {
        new_mode = 1 + rand()%NUM_MODES;       // modes start at 1.
        if (rand()%25==0) new_mode = 7;
        if (rand()%25==0) new_mode = 5;

        y_map_pos = -1;    // start new map generating
    }
    else
    {
        int a = rand() % modeprefs_total;
        int b = 0;
        int i = 1;
        while (i<=NUM_MODES)
        {
            b += modeprefs[i];
            if (a<b) 
            {
                new_mode = i;
                i = 9999;
                y_map_pos = -1;    // start new map generating
            }
            i++;
        }
    }
}

void FX_Apply_Mode(bool bLoadPreset, int iPresetNum)         // sets up DATA_X and other misc. stuff based on 'mode'
{
    // to start a new map background-generating, call this function
    // with y_map_pos = FX_YCUT.

    if (y_map_pos == -1)   // finished a map on last frame...
    {
        y_map_pos = FX_YCUT*FXW;    // so start a new one on this frame.
    }

    if (y_map_pos < FX_YCUT*FXW || y_map_pos > (FXH-FX_YCUT)*FXW)
    {
        dumpmsg("FATAL ERROR: FX_Apply_Mode(): received y_map_pos that was out of range");
        exit(99);
    }


    //if (g_rush_map) 
    //{
    //    temp_clock = clock();
    //}


    uint32_t a, b, addr, z1, z2;
    int32_t k, k1, k2;
    float newx, newy, newx2, newy2;
    int32_t x, y, n;
    uint32_t fx, z, A_number, D_offset;
    float tx, ty, d, e, f, r;
    int A_offset, R_offset_rel;//R_offset, old_R_offset are global;
    float inv_FXW = 2.0/FXW;
    //float inv_FXH = 2.0/FXH;
    float half_FXW = 0.5*FXW;
    //float half_FXH = 0.5*FXH;
    int bytewidth = (iDispBits==8) ? 1 : 4;

//    new_mode = 3;

    //float MAX_R_INV = 1.0/sqrtf(FXW*FXW+FXH*FXH);
    //int flag=0;   // big big

    if (y_map_pos == FX_YCUT*FXW)
    {
        g_bLoadPreset = bLoadPreset;
        g_iPresetNum = iPresetNum;

        if (!bLoadPreset)
        {
            new_gXC = FXW/2-1 + rand()%60 - 30;
            new_gYC = FXH/2-1 + rand()%30 - 15;

            weightsum = 256;
            
            new_damping = suggested_damping;
            if (new_damping < 0.50) new_damping = 0.50;
            if (new_damping > 1.00) new_damping = 1.00;
            if (mode_motion_dampened[new_mode]) new_damping *= 0.5;

            do
            {
                new_waveform = (rand() % (NUM_WAVES*3-1))/3 + 1;    // de-emphasizes last wave
                //waveform = (rand() % NUM_WAVES) + 1;        // all waves equal
            }
            while ((new_mode==6 && new_waveform==5) ||
                   (new_mode==12 && (new_waveform==4 || new_waveform==6)) || 
                   (new_mode==14 && (new_waveform==3 || new_waveform==4)) ||
                   ((new_mode==8 || new_mode==23 || new_mode==24) && new_waveform==6) 
                   );
        }
        fBigBeatThreshold = 1.10;
        //new_bScrollMap = false;


        frames_this_mode = 0;
        frames_crunching_this_mode = 0;

        if (new_mode==1) {                    // ***
            //scale1 = 0.97 + 0.015*((rand()%1000)*0.001);
            scale1 = 0.985 - 0.12*powf((rand()%1000)*0.001, 2);
            scale2 = scale1;
            turn1 = 0.01 + 0.01*((rand()%1000)*0.001);
            turn2 = turn1;

            //if (rand()%3==1) turn1 *= -1;
            if (scale1 > 0.97 && rand()%3==1) turn1 *= -1;

        } else if (new_mode==2) {                    // ****
            scale1 = 1.00 + 0.02*((rand()%1000)*0.001);
            turn1 =  0.02 + 0.07*((rand()%1000)*0.001);
        } else if (new_mode==3) {    // terra-landing new_mode
            scale1 = 0.85 + 0.1*((rand()%1000)*0.001); 
            scale2 = scale1;                           
            turn1 = 0.01 + 0.015*((rand()%1000)*0.001);
            turn2 = turn1;
        } else if (new_mode==4) {
            //scales will be handled by the map generator - change w/dist from center
            turn1 = 0.007 + 0.02*((rand()%1000)*0.001);   // normal
            turn2 = turn1;//0.007 + 0.02*((rand()%1000)*0.001);
        } else if (new_mode==5) {    // *****   SUPER-perspective
            //scales will be handled by the map generator - change w/dist from center
            turn1 = 0.01 + 0.03*((rand()%1000)*0.001);
            turn2 = turn1;//0.01 + 0.01*((rand()%1000)*0.001);
        } else if (new_mode==6) {        // VORTEX THINGY
            f = 0;
            int iNumPushTypes = 3;//((rand()%8)==0) ? 1 : 3;

            for (x=0; x<10; x++)
            {
                cx[x] = (rand() % (FXW*10))*0.1;
                cy[x] = FX_YCUT + (rand() % ((FXH-FX_YCUT*2)*10)  )*0.1;
                d = (rand()%628)*0.01;
                f = 1 + (rand()%80)*0.01;
                ci[x] = cosf(d)*f;
                cj[x] = sinf(d)*f;
                ctype[x] = rand()%iNumPushTypes;
                cr[x] = 80 + (rand() % 1200)*0.1;  // [80:200]
                cr_inv[x] = 1.0/cr[x];
            }

            //new_bScrollMap = rand() % 2;

            //frames_til_auto_switch *= 0.5;

        } else if (new_mode==7) {    // fuzzy
            //scales will be handled by the map generator
            turn1 = 0.01 + 0.01*((rand()%1000)*0.001);
            turn2 = turn1;//0.01 + 0.01*((rand()%1000)*0.001);
        } else if (new_mode==8) {  // ripples
            //scales will be handled by the map generator
            turn1 =  0.05*(0.001*(rand()%1000));
            turn2 = turn1;// 0.05*(0.001*(rand()%1000));    
        } else if (new_mode==9) {    // crazy-ass feedback (flower petals in 320)
            scale1 = 0.8 + 0.25*((rand()%1000)*0.001);
            scale1 = (scale1 - 1)*protective_factor + 1;
            scale2 = scale1;
            turn1 = 0.01 + 0.03*((rand()%1000)*0.001);
            turn2 = turn1;
        } else if (new_mode==10) {
            
            // no init

        } else if (new_mode==11) {
            scale1 = 1.008 + 0.008*((rand()%1000)*0.001);
            scale2 = scale1;
            turn1 = 0.12 + 0.06*((rand()%1000)*0.001);
            turn2 = turn1;

            //if (rand()%2==1) { 
                turn1 *= -0.6; 
                turn2 *= 0.1; 
                scale1 *= 0.99; 
                scale2 *= 1.01; 
            //}
        }
        else if (new_mode==12) {
            // horizontal streaking, out from centerline
            weightsum = weightsum*0.98;
        } 
        else if (new_mode==13) {
            //black hole - in @ center, out @ edges
            //scales will be handled by the map generator - change w/dist from center
            turn1 = 0.007 + 0.02*((rand()%1000)*0.001);
            turn2 = turn1;//0.007 + 0.02*((rand()%1000)*0.001);
        }
        else if (new_mode==14) {
            turn1 = 0.007 + 0.02*((rand()%1000)*0.001);   // normal
            turn2 = turn1;//0.007 + 0.02*((rand()%1000)*0.001);
        }
        else if (new_mode==15) {
            turn1 = 0.04*((rand()%1000)*0.001) + 
                    0.045*((rand()%1000)*0.001);
            turn2 = turn1;//0.01 + 0.01*((rand()%1000)*0.001);
            //weightsum *= 0.95;
        }
        else if (new_mode==16) {
            // Crystal ball
            turn1 = 0.007 + 0.02*((rand()%1000)*0.001);   // normal
            turn2 = turn1;//0.007 + 0.02*((rand()%1000)*0.001);
        }
        else if (new_mode==16) {
            weightsum = weightsum*(248.0/253.0); //240;
        }
        else if (new_mode >= 17)
        {
            turn1 = 0.007 + 0.02*((rand()%1000)*0.001);   // normal
            turn2 = turn1;//0.007 + 0.02*((rand()%1000)*0.001);
        }


        // randomly switch rotation direction
        if (rand()%2==1) { turn1 *= -1; turn2 *= -1; }


        f1 = 0.92 + 0.05*((rand()%1000)*0.001);
        f2 = 0.0009 + 0.0012*((rand()%1000)*0.001);

        if (new_mode==5)
        {
            //f1 = 0.98 + 0.018*((rand()%1000)*0.001);
            //f2 = 0.7 + 0.6 * ((rand()%1000)*0.001);
            //f1 = 0.92 + 0.01*((rand()%1000)*0.001);
            //f2 = 0.0009 + 0.0012*((rand()%1000)*0.001);

            //f1 = 0.05 + 0.05*((rand()%1000)*0.001) + 0.05*((rand()%1000)*0.001);
            //f2 = 0.99 - 0.01*((rand()%1000)*0.001) - 0.01*((rand()%1000)*0.001);
            f1 = 0.05 + 0.05*((rand()%1000)*0.001) + 0.07*((rand()%1000)*0.001);
            f2 = 0.99 - 0.01*((rand()%1000)*0.001) - 0.02*((rand()%1000)*0.001);
        }
        else if (new_mode==7)
        {
            //f1 = 0.96 + 0.01*((rand()%1000)*0.001);  // overall outward push (move) (lower = faster)
            //f2 = 0.03 + 0.025*((rand()%1000)*0.001);
            f1 = 0.92 + 0.01*((rand()%1000)*0.001);
            f2 = 0.0006 + 0.0005*((rand()%1000)*0.001);
        }
        else if (new_mode==8)
        {
               f1 = (0.001*(rand()%1000));
            f1 *= f1;
            f1 *= f1;
            f1 *= 8;        //16
            f1 += 1.5;   //1.5
        }
        else if (new_mode==9)
        {
            f1 = 0.98 + 0.01*((rand()%1000)*0.001);
            f2 = 0.0009 + 0.0012*((rand()%1000)*0.001);
        }
        else if (new_mode==13)
        {
            // linear scaling factor of final 'scale1' value from central value of 1..
            f1 = 0.92 + 0.16*((rand()%1000)*0.001);
        }
        else if (new_mode==15)
        {
            //f1 = -16 + (rand()%2)*(1024 + 32);
            /*
            f1 = 0.02 + 0.05*((rand()%1000)*0.001) + 0.05*((rand()%1000)*0.001);
            f2 = 0.02 + 0.05*((rand()%1000)*0.001) + 0.05*((rand()%1000)*0.001);
            f3 = 0.03 + 0.07*((rand()%1000)*0.001) + 0.07*((rand()%1000)*0.001);
            f4 = 0.03 + 0.07*((rand()%1000)*0.001) + 0.07*((rand()%1000)*0.001);
            */
            f1 = rand()%5 + 2;                // number of petals
            f2 = 0.92 + 0.06*((rand()%1000)*0.001);   // scale, lower bound
            f3 = 0.05 + 0.05*((rand()%1000)*0.001);   // scale, range
        }
        else if (new_mode==17)
        {
            f1 = 0.01 + 0.09*((rand()%1000)*0.001);
            f2 = 0.01 + 0.09*((rand()%1000)*0.001);
            f3 = 0.01 + 0.09*((rand()%1000)*0.001);
            f4 = 0.01 + 0.09*((rand()%1000)*0.001);
        }

        turn1 *= 0.6;
        turn2 *= 0.6;

        R_offset = 0;

        max_rad_inv = 1.0/sqrtf(FXW*FXW+FXH*FXH);
        rmult = 640.0/(float)FXW;
        rdiv = 1.0/rmult;
        protective_factor = (FXW > 640) ? 640.0/(float)FXW : 1;

        if (bLoadPreset)
        {
            char section[64];
            sprintf(section, "PRESET %d", iPresetNum);
            char temp[64];
            GetPrivateProfileString(section,"t1","1",temp,64,inifile); sscanf(temp, "%f", &turn1);
            GetPrivateProfileString(section,"t2","1",temp,64,inifile); sscanf(temp, "%f", &turn2);
            GetPrivateProfileString(section,"s1","1",temp,64,inifile); sscanf(temp, "%f", &scale1);
            GetPrivateProfileString(section,"s2","1",temp,64,inifile); sscanf(temp, "%f", &scale2);
            GetPrivateProfileString(section,"f1","1",temp,64,inifile); sscanf(temp, "%f", &f1);
            GetPrivateProfileString(section,"f2","1",temp,64,inifile); sscanf(temp, "%f", &f2);
            GetPrivateProfileString(section,"f3","1",temp,64,inifile); sscanf(temp, "%f", &f3);
            GetPrivateProfileString(section,"f4","1",temp,64,inifile); sscanf(temp, "%f", &f4);
        }

        cos_turn1 = cosf(turn1);
        sin_turn1 = sinf(turn1);
        cos_turn2 = cosf(turn2);
        sin_turn2 = sinf(turn2);

        if (FXW*FXH <= 320*240)
            weightsum_res_adjusted = weightsum*250/256;
        else if (FXW*FXH <= 400*300)
            weightsum_res_adjusted = weightsum*251/256;
        else if (FXW*FXH <= 512*384)
            weightsum_res_adjusted = weightsum*252/256;
        else if (FXW*FXH <= 800*600)
            weightsum_res_adjusted = weightsum*253/256;
        else if (FXW*FXH <= 1280*960)
            weightsum_res_adjusted = weightsum*254/256;
        else //if (FXW*FXH <= 1440*900)
            weightsum_res_adjusted = weightsum*255/256;
        //else
      //    weightsum_res_adjusted = weightsum*256/256;  //BAD - DON'T USE 256 - IT LANDS ON ZERO!
    

        frames_til_auto_switch = frames_til_auto_switch__registry;
        if (fps >= 10.0f && fps < 120.0f)
            frames_til_auto_switch = frames_til_auto_switch__registry * fps/30.0f;

        if (bLoadPreset) dumpmsg("preset loaded... applying");
    }
    
    


    // generate a portion of new map
    if (y_map_pos < (FXH-FX_YCUT)*FXW)
    {
        // for mode 6 stuff
        float i,j,z;  
        int h;
        float xxyy;
        int lo = 1;//(new_bScrollMap) ? 0 : 1;
        int hi = 3;//(new_bScrollMap) ? 2 : 3;

        frames_crunching_this_mode++;

        float new_damping_temp = new_damping;
        // when FPS is high, slow down the motion to match:
        if (fps_at_last_mode_switch >= 10.0f && fps_at_last_mode_switch <= 120.0f)
            new_damping_temp *= 30.0f / fps_at_last_mode_switch;
        // when screen is large, in modes 5 and 7, speed up the motion a bit:
        //if (FXW > 640 && (new_mode==5 || new_mode==7))
        //    new_damping_temp *= powf(FXW/640.0f, 0.23f);
        //else 
        //if (FXW > 800) // slow down the other modes, @ higher resolutions.
        //    new_damping_temp *= powf(800.0f/FXW, 0.25f);

        //if (frames_crunching_this_mode > 305) exit(15);

        // note: 'frames_til_auto_switch' has been fudged, if we aren't running at 30fps, based on 'frames_til_auto_switch__registry'.
        int end_pos = FXW*(FX_YCUT + ((float)frames_crunching_this_mode/(float)frames_til_auto_switch)*(float)(FXH-FX_YCUT*2));
        if (g_rush_map || end_pos > FXW*(FXH-FX_YCUT))
        {
            end_pos = FXW*(FXH-FX_YCUT);
        }

        //dumpmsg("endpos=%d", end_pos);

        y = y_map_pos / FXW;
        x = y_map_pos % FXW;

        for ( ; y_map_pos < end_pos; y_map_pos++)
        { 

            A_offset = ((int32_t)y*FXW + (int32_t)x) * 8;
            old_R_offset = R_offset;

            //R_offset to be determined            // dest. read pixel offset: 0..63999, 0 for "null"
            //A_offset                            // for the array:           0..FXW*240-1

            newx2 = (x - new_gXC);   
            newy2 = (y - new_gYC);   

            //---------------------------------------------------------------------------------------------------
        
            if (new_mode <= 16)
            {
                half_FXW = 1.0;   // DANGER!

                if (new_mode==3)
                {
                    scale1 = 0.95 - (newy2*(480.0/FXH))*0.0005;
                    //scale1 = 0.93 - newy2*0.001;
                    //scale2 = scale1;
                }
                else if (new_mode==4)
                {
                    // normal mode 4 sphere:
                    r = sqrtf(newx2*newx2 + newy2*newy2);
                    r *= rmult;
                    scale1 = 0.9 + r*0.0025*0.14;//scale1 = 0.9 + r*0.0025*0.14;

                    // the crystal ball effect:
                    //r = sqrtf(newx2*newx2 + newy2*newy2);
                    //r *= rmult;
                    //scale1 = 1.05 - r*r*0.00025*0.07;//scale1 = 0.9 + r*0.0025*0.14;

                    // the AWESOME up-down split-world warp:
                    //turn1 = 0.3*sinf(newx2*12.0f/(float)FXW);
                    //scale1 = 0.9 + 0.2*cosf(newy2*12.0f/(float)(FXH + (rand() & 1023)/1024.0f));

                    //scale2 = scale1;
                }
                else if (new_mode==5)
                {
                    //r = sqrtf(sqrtf(newx2*newx2 + newy2*newy2))*0.19;
                    //scale1 = f2 - f1*(  r  );      // was 0.98 - 0.1*(  r  ) originally
                    //scale1 = (scale1 - 1)*protective_factor + 1;
                
                

                
                    r = sqrtf(newx2*newx2 + newy2*newy2);
                    r *= (1.0/200.0);
                    r *= rmult;
                    if (effect[NUCLIDE] == -1) 
                        r = sqrtf(r); 
                    else 
                        r *= 1.7;
                    scale1 = f2 - f1*(  r  );      // was 0.98 - 0.1*(  r  ) originally
                    scale1 = (scale1 - 1)*protective_factor + 1;
                    //scale2 = scale1;
                
                    /*
                    // f2 = 1.0 avg;   in [0.7 ... 1.3]
                    r = sqrtf(newx2*newx2 + newy2*newy2)*f2;
                    r *= MAX_R_INV;
                      // now r is from 0..1
                    r = sqrtf(r);
                    scale1 = f1 - r*0.1;
                    scale2 = scale1;
                    */
                }
                else if (new_mode==7)
                {
                    r = sqrtf(newx2*newx2 + newy2*newy2)*f2;
                    r *= rmult;
                    scale1 = (f1 - r);
                    scale1 = (scale1 - 1)*protective_factor + 1;
                    //scale2 = scale1;
                    scale1 += rand_array[rand_array_pos++]; //(rand() % 100)*0.0005;
                    if (rand_array_pos >= 2345) rand_array_pos = 0;

                    //scale2 += (rand() % 100)*0.0005;

                    /*
                    r = sqrtf(newx2*newx2 + newy2*newy2);
                    r *= MAX_R_INV;
                      // now r is from 0..1
                    scale1 = f1 - r*f2;
                    scale2 = scale1;
                    scale1 -= (rand() % 100)*0.0004;
                    scale2 -= (rand() % 100)*0.0004;
                    */
                }            
                else if (new_mode==8)
                {
                    r = sqrtf(newx2*newx2 + newy2*newy2);
                    r *= rmult;
                    scale1 = 0.85 + 0.1*sinf(sqrtf(r)*f1);
                    //scale2 = scale1;
                }
                else if (new_mode==9)
                {
                    r = sqrtf(newx2*newx2 + newy2*newy2)*f2;
                    r *= rmult;
                    scale1 = f1 - r;
                    scale1 = (scale1 - 1)*protective_factor + 1;
                    //scale2 = scale1;

                    // get angle into r
                    //r = atan2(newy2, newx2);
                    //scale1 *= 1.0 + f3*sinf(r*p1);  // p1: number of petals
                    //scale2 = scale1;
                }
                else if (new_mode==13)
                {
                    //r = sqrtf(newx2*newx2 + newy2*newy2);
                    //r *= rmult;
                    //scale1 = 1.1 - r*0.0025*0.14;

                    // AWESOME continuous black hole
                    // sucks in @ center, pushes out @ edges, SUPER SMOOTH!!!!!!!
                    r = sqrtf(newx2*newx2 + newy2*newy2);
                    r *= rmult;
                    scale1 = 1.04 - r*sqrtf(r)*0.00025*0.14;//scale1 = 0.9 + r*0.0025*0.14;
                    scale1 = (scale1 - 1)*f1 + 1;
                }
                else if (new_mode==14)
                {
                    // the AWESOME up-down split-world warp:
                    //turn1 = 0.3*sinf(newx2*12.0f/(float)FXW);
                    //cos_turn1 = cosf(turn1);
                    //sin_turn1 = sinf(turn1);
                    scale1 = 0.9 + 0.2*cosf(newy2*12.0f/(float)(FXH + (rand() & 1023)/1024.0f));
                }
                else if (new_mode==15)
                {
                    scale1 = f2 + f3*sinf(  atan2(newy2, newx2)*f1  );
                }
                else if (new_mode==16)
                {
                    // the crystal ball effect:
                    // UNDER CONSTRUCTION
                    r = sqrtf(newx2*newx2 + newy2*newy2);
                    r *= rmult;///640.0f;

                    //scale1 = 1.05 - r*r*0.00025*0.07;//scale1 = 0.9 + r*0.0025*0.14;
                    scale1 = 1.05 - r*r*0.00025*0.09;//scale1 = 0.9 + r*0.0025*0.14;
                    //if (scale1 < -1.05) scale1 = -1.05;
                    if (scale1 < -1.5) scale1 = -1.5;


                    //scale1 = 1 + (scale1 - 1) * fabs(1-newy2*2.0f/(float)FXH);   // 0..1 - 0 @ center, 1 @ edge
                }
            }
            else // from 17 and up
            {
                //newx2 = (x - new_gXC)*inv_FXW;   // from -1..1, ROUGHLY (can be outside)
                //newy2 = (y - new_gYC)*inv_FXW;   // from -1..1, ROUGHLY (can be outside)
                newx2 *= inv_FXW;   // from -1..1, ROUGHLY (can be outside)
                newy2 *= inv_FXW;   // from -1..1, ROUGHLY (can be outside)

                if (new_mode==17)
                {
                    // mode 1: horz. tunnel
                    scale1 = 0.97 - newy2*newy2*0.40;  
                    //      p1: range 0.96..0.98 (overall speed)
                    //      p2: range 0.30..0.45 (extremeness of curvature)
                }
                else if (new_mode==18)
                {
                    // mode 2: vert. tunnel
                    scale1 = 0.97 - newx2*newx2*0.40;  
                    //      p1: range 0.96..0.98 (overall speed)
                    //      p2: range 0.30..0.45 (extremeness of curvature)
                }
                else if (new_mode==19)
                {
                    // mode 5: vortex (old mode 12)
                    scale1 = 1.04 - 0.25*sqrtf(newx2*newx2 + newy2*newy2);
                    //      p1: range 1.03..1.05 (suck-in speed @ center)
                    //      p2: range 0.15..0.30 (outward speed @ edges)
                }
                else if (new_mode==20)
                {
                    // mode 7: terra''
                    scale1 = 1.15 - sqrtf(newy2 + 1.4)*0.20;
                }
                else if (new_mode==21)
                {
                    // regular zoom-in but w/diced cube look (gritty, dirty electronics; shakes;)
                    scale1 = 0.95 - ((int)(fabsf(newx2)*10))*0.03 - ((int)(fabsf(newy2)*10))*0.03;
                }
                else if (new_mode==22)
                {
                    // ***AWESOME*** - phonic rings - scale is dicey based on r
                    scale1 = 0.95 - ((int)(sqrtf(newx2*newx2 + newy2*newy2)*10))*0.04;
                }
                else if (new_mode==23)
                {
                    // BADASS derivative of phonic rings - quicker response & fadeout!
                    scale1 = 0.95 - (((int)(sqrtf(newx2*newx2 + newy2*newy2)*20)) % 4)*0.12;
                }
                else if (new_mode==24)
                {
                    // mode 11: fast swirl, looks very good
                    scale1 = 0.96;  // range: 0.95 - 0.97
                    turn1 = 0.05;   // range: 0.04 - 0.06
                    cos_turn1 = cosf(turn1);
                    sin_turn1 = sinf(turn1);
                }
                else if (new_mode==25)
                {
                    // WOW: 1/r scaling... VERY good zoom, and fully speed-scalable (no curl!)
                    scale1 = 3.0 / (3.0 + sqrtf(newx2*newx2 + newy2*newy2)); // param1==param2: lower = faster.  1.0 is pretty fast already.  2.0 should probably be min; 6.0, max (very smooth)
                                // -use 0.3,0.3 for ultra-fast zoom, looks killer
                }
            }


                /*
                if (new_mode <= 24)
                {
                    if (new_mode==17)
                    {
                        // mode 0: flat zoom
                        scale1 = 0.99;
                        //      p1: range 0.86..0.99 (overall speed) (prefer 0.99)
                    }
                    else if (new_mode==20)
                    {
                        // mode 3: typical tunnel
                        scale1 = 0.9 - 0.10*sqrtf(newx2*newx2 + newy2*newy2);
                        //      p1: range 0.90..0.99 (overall speed)
                        //      p2: range 0.05..0.20 (extremeness of curvature)
                    }
                    else if (new_mode==21)
                    {
                        // mode 4: tunnel #2
                        scale1 = 0.95 - 0.30*(newx2*newx2 + newy2*newy2);
                        //      p1: range 0.93..0.97 (overall speed @ center)
                        //      p2: range 0.05..0.30 (additional speed @ edges)
                    }
                    else if (new_mode==23)
                    {
                        // mode 6: terra'
                        scale1 = 1.15 - (newy2 + 1.0)*0.20;
                    }
                }
                else // from 25-32
                {
                    if (new_mode==25)
                    {
                        // mode 8: sphere' - more like a sphere
                        scale1 = min(1.05, 0.90 + 0.25*(newx2*newx2 + newy2*newy2));
                    }
                    else if (new_mode==26)
                    {
                        // mode 9: sphere'' - more like a gaussian bump - smaller sphere
                        scale1 = min(1.1, 0.90 + 0.20*sqrtf(newx2*newx2 + newy2*newy2));
                    }
                    else if (new_mode==27)
                    {
                        // alternate #9: a diamond
                        scale1 = min(1.1, 0.90 + 0.15*(fabsf(newx2) + fabsf(newy2)));
                    }
                    else if (new_mode==28)
                    {
                        // hourglass
                        scale1 = min(1.1, 0.90 + 0.15*(fabsf(newx2) - fabsf(newy2)));
                    }
                    else if (new_mode==29)
                    {
                        // hall of mirrors
                        scale1 = 1.0 + 0.5*fabsf(newx2);
                    }
                    else if (new_mode==30)
                    {
                        // alternate to petals; looks incredibly SMOOTH for some reason
                        scale1 = 1.0 - 0.3*fabsf(newx2) - 0.3*fabsf(newy2) + 0.3*sqrtf(newx2*newx2 + newy2*newy2);
                    }
                }
                else if (new_mode==34)
                {
                    // so-so... scale is normal zoom, but turning is sinf(angle * int)
                    scale1 = 12.0 / (12.0 + sqrtf(newx2*newx2 + newy2*newy2)); // param1==param2: lower = faster.  1.0 is pretty fast already.  2.0 should probably be min; 6.0, max (very smooth)
                    turn1 = 0.03 + 0.02 * sinf(atan2(newx2,newy2) * 3.0 + 1.7);  // random phase; w = 2 or 3 (integers!)
                    cos_turn1 = cosf(turn1);
                    sin_turn1 = sinf(turn1);
                }
                else if (new_mode==35)
                {
                    // really good too; central spherical view, then edges get cut & propagated out
                    scale1 = 0.95; if (sqrtf(newx2*newx2 + newy2*newy2) > 0.5) scale1 = 0.9;
                }
                else if (new_mode==37)
                {
                    // mode 10: AWESOME super-fast stretch-to-death zoom
                    scale1 = 1.0 - 0.45*sqrtf(newx2*newx2 + newy2*newy2);
                    //      p1: 1.0
                    //      p2: 0.4..0.5 (extremeness... 0.5 brings slight curling @ edges)
                }
                */
            
            if (custom_motion_vectors[new_mode])
            {
                if (new_mode==6)
                {
                    tx = 0;
                    ty = 0;
                    f = 0;

                    for (n=0; n<5; n++)
                    {
                        if (ctype[n]==0)
                        {
                            newx = cx[n]-x;
                            newy = cy[n]-y;

                            d = (newx*newx + newy*newy);
                            //d *= rdiv;
                            d = 1.0/(d+0.1);
                            f += d;

                            tx += ci[n] * d;
                            ty += cj[n] * d;
                        }
                        else if (ctype[n]==1)
                        {
                            newx = cx[n]-x;
                            newy = cy[n]-y;

                            xxyy = (newx*newx + newy*newy);
                            d = xxyy;//*rdiv;
                            d = 1.0/(d+0.1);
                            f += d;

                            z = sqrtf(xxyy);
                            z = 1.0/(z+0.01);
                            
                            d += d;

                            tx += d*(-newy)*z;
                            ty += d*(newx)*z;
                        }
                        else if (ctype[n]==2)
                        {
                            newx = cx[n]-x;
                            newy = cy[n]-y;

                            xxyy = (newx*newx + newy*newy);
                            d = xxyy;//*rdiv;
                            d = 1.0/(d+0.1);
                            f += d;

                            z = sqrtf(xxyy);
                            z = 1.0/(z+0.01);
                            
                            d += d;

                            tx += d*(newy)*z;
                            ty += d*(-newx)*z;
                        }
                    }


                    if (f > 0.000001)
                    {
                        f = 1.9/f;
                        tx = tx*f;
                        ty = ty*f;
                    }
                    else
                    {
                        tx = 0;
                        ty = 0;
                    }
                
                    //tx -= (newx2*0.005); //0.002
                    //ty -= (newy2*0.005);

                    newx = x + tx - 0.1;
                    newy = y + ty + 0.6;
                    
                }
                else if (new_mode==10)
                {
                    newx = newx2*(1.03 + 0.03*(y/(float)FXH)     ) + new_gXC;
                    newy = y*1.04;
                } 
                else if (new_mode==12)   // sideways splitter - custom vectors!
                {
                    if (newx2 < -0.5)
                        newx = -sqrtf(-newx2) + new_gXC + 0.9;
                    else if (newx2 > 0.5)
                        newx = sqrtf(newx2) + new_gXC - 0.9;
                    else 
                        newx = new_gXC;

                    newy = newy2 + new_gYC;
                }
            }
            else if (rotation_dither[new_mode])
            {
                // vectors based on turn1 and scale1, and turn2 and scale2
                if ((x%2) == (y%2))
                {
                    newx = newx2*(cos_turn1) - newy2*(sin_turn1) ;    //Z rotation
                    newy = newx2*(sin_turn1) + newy2*(cos_turn1) ;
                    newx = (newx)*scale1*half_FXW + new_gXC;// + x_trans;
                    newy = (newy)*scale1*half_FXW + new_gYC;// + y_trans;
                }
                else
                {
                    newx = newx2*(cos_turn2) - newy2*(sin_turn2) ;    //Z rotation
                    newy = newx2*(sin_turn2) + newy2*(cos_turn2) ;
                    newx = (newx)*scale2*half_FXW + new_gXC;// + x_trans;
                    newy = (newy)*scale2*half_FXW + new_gYC;// + y_trans;
                }
            }
            else    // vectors based on turn1 and scale1
            {
                newx = newx2*(cos_turn1) - newy2*(sin_turn1);    //Z rotation
                newy = newx2*(sin_turn1) + newy2*(cos_turn1);
                newx = (newx)*scale1*half_FXW + new_gXC;// + x_trans;
                newy = (newy)*scale1*half_FXW + new_gYC;// + y_trans;
            }




            // DEBUG: point to exact same pixel.
            //newx = newx2 + new_gXC;
            //newy = newy2 + new_gYC;

            // NOTICE: the look-at pixel can be virtually
            // anywhere on X, but it is restrained to within
            // +/- 32767 pixels of the base pixel!
            // (due to storage of R_offset_rel)

          

            //if (SoundActive && !SoundEmpty && new_mode != 6)// && newx >= 0 && newx <= FXW-1)
            //if (new_damping_temp < 0.99)
            {
                newx = x*(1.0f - new_damping_temp) + newx*new_damping_temp;
                newy = y*(1.0f - new_damping_temp) + newy*new_damping_temp;
                //newx = (x + newx)*0.5;
                //newy = (y + newy)*0.5;

                while (newx < 0.0)    newx += FXW-1;
                while (newx > FXW-1) newx -= FXW-1;
            }
            /*else
            {
                while (newx < 0)   newx += FXW;
                while (newx > FXW) newx -= FXW;
            }*/

            /*
            while (newy < FX_YCUT) newy += (FXH-FX_YCUT*2 - 1);
            while (newy > FXH - FX_YCUT - 1) newy -= (FXH-FX_YCUT*2 - 1);
            */

            /*
            if (mode_motion_dampened[new_mode])
            {
                newx = (x + newx)*0.5;
                newy = (y + newy)*0.5;

                while (newx < 0.0)    newx += FXW-1;
                while (newx > FXW-1) newx -= FXW-1;
            }
            else
            {
                while (newx < 0)   newx += FXW;
                while (newx > FXW) newx -= FXW;
            }
            */

            /*
            // New in 4.26:
            if (newy < 0)
                newy = 0;
            if (newy > FXH-1)
                newy = FXH-1;
            if (newx > 0)
                newx -= ((int)newx/FXW)*FXW;
            else
                newx += ((-1 - (int)newx)/FXW + 1)*FXW;
            */

            a = (int)newx;
            b = (int)newy;
            R_offset = b*FXW + a;

            if (R_offset < FXW*2) 
                R_offset = FXW*2;
            if (R_offset >= FXW*(FXH-3)-1)
                R_offset =  FXW*(FXH-3)-1;

            R_offset_rel = R_offset - old_R_offset;    //- old_R_offset;//((int32_t)y*320L+(int32_t)x);        // store offset RELATIVE TO THIS PIXEL

            //int dist_from_center_squared = (a-FXW/2)*(a-FXW/2) + (b-FXH/2)*(b-FXH/2);
            int weightsum_this_pixel = weightsum_res_adjusted;//Diminish_Center() alternate:  - max(0,3 - 3*dist_from_center_squared/300);


            //AHA: must save room for +/- FXW*2 to be added on there, due to sliders...
            //Keep in mind that R_offset starts at 0 at the very top pixel.
            //if ( newy < 0 || newy >= FXH || R_offset_rel < 5 || R_offset_rel > 65530 )
            //if ( newy < 0 || newy > FXH-1 || R_offset_rel < 5 || R_offset_rel > 65530 )
            //if ( newy < FX_YCUT || newy > FXH-FX_YCUT || R_offset_rel < 5 || R_offset_rel > 65530 )
            //if ( newy < 1 || newy >= FXH-1)// || R_offset_rel < 5 || R_offset_rel > 65530 )  // changed so we could do some dynamic vertical shifting w/maps
            /*if (R_offset < FXW*2 || R_offset >= FXW*(FXH-3))
            {
                R_offset = old_R_offset;//A_offset/6;
                R_offset_rel = 0;//32767;// + R_offset - old_R_offset;    //- old_R_offset;//((int32_t)y*320L+(int32_t)x);        // store offset RELATIVE TO THIS PIXEL
                // the following is normally +0 +1 +2 +3
                DATA_FX2[A_offset + 0] = (unsigned char)(weightsum_res_adjusted/4);  // stores (0-255) TOPLEFT's weight
                DATA_FX2[A_offset + 1] = (unsigned char)(weightsum_res_adjusted/4);  // stores (0-255) TOPRIGHT's weight
                DATA_FX2[A_offset + 2] = (unsigned char)(weightsum_res_adjusted/4);  // stores (0-255) BTMLEFT's weight
                DATA_FX2[A_offset + 3] = (unsigned char)(weightsum_res_adjusted/4);  // stores (0-255) BTMRIGHT's weight
                ((int *)(DATA_FX2))[(A_offset + 4)/4] = R_offset_rel * bytewidth;
            }
            else*/
            {
                //R_offset = (int32_t)b*FXW + (int32_t)a;
                //R_offset_rel = 32767 + R_offset - old_R_offset;    //- old_R_offset;//((int32_t)y*320L+(int32_t)x);        // store offset RELATIVE TO THIS PIXEL
                /*if ((R_offset_rel < 0) || (R_offset_rel > 65535L)) 
                {
                    char buf[80];
                    wsprintf(buf, "R_offset_rel overflow!");
                    MessageBox( NULL, buf, "NOTICE", MB_OK );
                }*/
                newx2 = (newx - a);
                newy2 = (newy - b);
                /*
                DATA_FX2[A_offset + 0] = (unsigned char)((1-newx2)*(1-newy2)*weightsum_res_adjusted);  // stores (0-255) TOPLEFT's weight
                DATA_FX2[A_offset + 1] = (unsigned char)((  newx2)*(1-newy2)*weightsum_res_adjusted);  // stores (0-255) TOPRIGHT's weight
                DATA_FX2[A_offset + 2] = (unsigned char)((1-newx2)*(  newy2)*weightsum_res_adjusted);  // stores (0-255) BTMLEFT's weight
                DATA_FX2[A_offset + 3] = (unsigned char)((  newx2)*(  newy2)*weightsum_res_adjusted);  // stores (0-255) BTMRIGHT's weight
                DATA_FX2[A_offset + 4] = (unsigned char)(R_offset_rel & 0xff);
                DATA_FX2[A_offset + 5] = (unsigned char)(R_offset_rel >> 8);
                */
                // the following is normally +0 +1 +2 +3
                DATA_FX2[A_offset + 0] = (unsigned char)((1-newx2)*(1-newy2)*weightsum_this_pixel);  // stores (0-255) TOPLEFT's weight
                DATA_FX2[A_offset + 1] = (unsigned char)((  newx2)*(1-newy2)*weightsum_this_pixel);  // stores (0-255) TOPRIGHT's weight
                DATA_FX2[A_offset + 2] = (unsigned char)((1-newx2)*(  newy2)*weightsum_this_pixel);  // stores (0-255) BTMLEFT's weight
                DATA_FX2[A_offset + 3] = (unsigned char)((  newx2)*(  newy2)*weightsum_this_pixel);  // stores (0-255) BTMRIGHT's weight
                ((int *)(DATA_FX2))[(A_offset + 4)/4] = R_offset_rel * bytewidth;
                //DATA_FX2[A_offset + 4] = (unsigned char)((R_offset_rel & 0x000000ff) >>  0);
                //DATA_FX2[A_offset + 5] = (unsigned char)((R_offset_rel & 0x0000ff00) >>  8);
                //DATA_FX2[A_offset + 6] = (unsigned char)((R_offset_rel & 0x00ff0000) >> 16);
                //DATA_FX2[A_offset + 7] = (unsigned char)((R_offset_rel & 0xff000000) >> 24);
                    //DATA_FX2[A_offset + 4] = (unsigned char)(R_offset_rel & 0xff);
                    //DATA_FX2[A_offset + 5] = (unsigned char)(R_offset_rel >> 8);
            }

            old_R_offset = R_offset;

            x++;
            if (x==FXW)
            {
                x = 0;
                y++;
            }

        }
    }


    // new map is finished generating in background; apply it

    //if (y_map_pos == (FXH-FX_YCUT)*FXW &&
    //    ((g_rush_map)    ||    (!bBeatMode || (bBeatMode && bBigBeat)))
    //    )
    if (y_map_pos == (FXH-FX_YCUT)*FXW &&
        !g_rush_map && 
        bBeatMode && 
        !bBigBeat
        )
    {
        fBigBeatThreshold -= 0.2 / (float)frames_til_auto_switch;
    }
    else if (y_map_pos == (FXH-FX_YCUT)*FXW &&
        ((g_rush_map)    ||    (!bBeatMode || (bBeatMode && bBigBeat)))
        )
    {
        //dumpmsg("applying new map");  //big big

        bLoadPreset = g_bLoadPreset;
        iPresetNum = g_iPresetNum;

        y_map_pos = -1;    // means map is finished & applied.

        mode = new_mode;
        damping = new_damping;
        gXC = new_gXC;
        gYC = new_gYC;
        waveform = new_waveform;
        //initial_map_offset = new_initial_map_offset;
        //bScrollMap = new_bScrollMap;

        if (fps >= 5.0f && fps <= 120.0f)
            fps_at_last_mode_switch = fps;

        if (bLoadPreset) dumpmsg("preset map generated.");

        old_f1 = f1;
        old_f2 = f2;
        old_f3 = f3;
        old_f4 = f4;
        old_weightsum = weightsum;
        old_turn1 = turn1;
        old_turn2 = turn2;
        old_scale1 = scale1;
        old_scale2 = scale2;

        bLocked = FALSE;

        // clear off-screen areas for new mode
        if (mode != 6)
        {
            k = max(0, FX_YCUT_HIDE-6);    // top line to go from
            x = min(6, FX_YCUT_HIDE);      // # of lines to clear
            if (iDispBits==8)
            {
                memset(&VS1[k*FXW],         0, FXW*x);
                memset(&VS1[(FXH-1-k-x)*FXW], 0, FXW*x);
                memset(&VS2[k*FXW],         0, FXW*x);
                memset(&VS2[(FXH-1-k-x)*FXW], 0, FXW*x);
            }
            else 
            {
                memset(&VS1[k*FXW*4],         0, FXW*x*4);
                memset(&VS1[(FXH-1-k-x)*FXW*4], 0, FXW*x*4);
                memset(&VS2[k*FXW*4],         0, FXW*x*4);
                memset(&VS2[(FXH-1-k-x)*FXW*4], 0, FXW*x*4);
            }
        }

        //if (RND==1) srand(time(NULL) + intframe);
        //frames_til_auto_switch = 550;    

        
        if (bLoadPreset)
        {
            char section[64];
            sprintf(section, "PRESET %d", iPresetNum);
            char temp[64];
            effect[CHASERS] = GetPrivateProfileInt(section,"effect_chasers",0,inifile);
            effect[BAR]     = GetPrivateProfileInt(section,"effect_bar"    ,0,inifile);
            effect[DOTS]    = GetPrivateProfileInt(section,"effect_dots"   ,0,inifile);
            effect[SOLAR]   = GetPrivateProfileInt(section,"effect_solar"  ,0,inifile);
            effect[GRID]    = GetPrivateProfileInt(section,"effect_grid"   ,0,inifile);
            effect[NUCLIDE] = GetPrivateProfileInt(section,"effect_nuclide",0,inifile);
            effect[SHADE]   = GetPrivateProfileInt(section,"effect_shadebobs",0,inifile);
            effect[SPECTRAL]= GetPrivateProfileInt(section,"effect_spectral",0,inifile);
            g_bSlideShift   = GetPrivateProfileInt(section,"shift",0,inifile);
        }
        else 
        {
            g_bSlideShift = (rand()%100 + 1) <= g_SlideShiftFreq;

            for (x=0; x<NUM_EFFECTS; x++)
            {
                int thresh = modeInfo[mode].effect_freq[x];
                if (SoundActive && !SoundEmpty) thresh *= 0.7;
                if (rand()%1000 < thresh)
                    effect[x] = 1;
                else
                    effect[x] = -1;
            }
            modeInfo[mode].Clip_Num_Effects();

            if (effect[CHASERS]==1)
            {
                effect[CHASERS] = 1 + rand()%2;
            }

            // if grid is on, turn bar off
            if (effect[GRID]==1) effect[BAR] = -1; /* turn heavy bar off */

            // initial sun effect
            if (mode==1 && rand()%2==0) Drop_Solar_Particles(500);

            grid_dir = (rand()%2)*2-1;
            if (effect[NUCLIDE]>0 && rand()%7>2) waveform = 0;

            #if PLUGIN
                visMode = wave;
            #endif

            if (new_mode==10) 
            {
                waveform = 1;
                #if PLUGIN
                    if (rand()%4 > 0) visMode = spectrum;
                #endif
            }

            if (new_mode==15 && rand()%5==0)
                waveform = 5;
            //if (new_mode==18) 
            //    waveform = rand()%2 + 1;

            if (effect[SPECTRAL]==1)
                waveform = 0;
        }

        if (!g_bSlideShift) { slider1 = 0; }

        solar_max = modeInfo[mode].solar_max;
        center_dwindle = modeInfo[mode].center_dwindle;

        // swap maps
        unsigned char *pFX;
        pFX = DATA_FX;
        DATA_FX = DATA_FX2;
        DATA_FX2 = pFX;

        if (!bLoadPreset && intframe > 0)
        {
            FX_Random_Palette();
        }

        /* more effect[SPECTRAL] STUFF:
        float sum;
        for (int i=0; i<NUM_FREQS; i++)
        do
        {
            g_DotColor[i][0] = (rand()%1000)*0.001;
            g_DotColor[i][1] = (rand()%1000)*0.001;
            g_DotColor[i][2] = (rand()%1000)*0.001;
            sum = g_DotColor[i][0] + g_DotColor[i][1] + g_DotColor[i][2];
        }
        while (sum < 1.2);
        */

        g_rush_map = false;  // finished rushing.

    }


    //if (g_rush_map) 
    //{
    //    clock_debt += (clock() - temp_clock);
    //}

    bLoadPreset = false;

    //g_pause = false;
}

void RenderFX()
{
    unsigned int addr;// a, b, z1, z2;
    int32_t k, k1;// k2;
    float newx, newy;
    unsigned char quitnow=0;
    //unsigned char p1, p2, p3;
    int x, y, dir;
    unsigned int fx, z0, A_offset, A_number;// z, R_offset;
    uint32_t arraypos;// D_offset;
    //unsigned char m;

    //g_FramesSinceShift++;

  SetCursor(NULL); //otherwise, in Winamp, you can hit 'c' or something and the cursor will pop up!

    floatframe += 1.6f * min(1.0f, 47.0f/fps);
    intframe++;
    frames_this_mode++;

    if ((intframe % 11) == 0)
      clearframes = 1;

    //dither/////AMDTEST
    //dither//#if AMDTEST
    //dither//    bDither = 1;
    //dither//    bRandomDither = 0;
    //dither//#endif
    //dither/////AMDTEST


/*
    if (iDispBits>8 && g_hit >= 0)    // g_hit at -1 means 'we got it at zero, don't bother again.'
    {
        for (x=0; x<256; x++) 
        {
            // this is only used for 16-bit non-MMX... disabling it so that g_hit is sync'd to title blits.
            //REMAP[x]  = min(255, x*(2 + g_hit*0.1));

            //REMAP2[x] = REMAP[x];
            //REMAP3[x] = REMAP[x];
        }
    }
*/

    if (iBlendsLeftInPal > 0)
    {
        iBlendsLeftInPal--;
        PutPalette();
    }

    //dither//if (bRandomDither)
    //dither//{
    //dither//    if (/*rand()%600==0 ||*/ (bMEGARandomDither && intframe%(int)(fps*1.5)==0))
    //dither//    {
    //dither//        bDither = !bDither;
    //dither//    }
    //dither//}

    //----------------------------------------------------------------------------------------------------------
    // VS1 is source, VS2 is dest.


    //------EFFECTS----------------------------------------------------------------------------------------------------

    //if ((frames_since_last_plop++ > 200) && (rand()%2500 == 1))
    //{
        //FX_Loadimage("fx.tga");
    //}

    if (effect[SHADE]>0) ShadeBobs();
    if (effect[CHASERS]>=1) Two_Chasers(floatframe + chaser_offset);
    if (effect[BAR] == 1)
    {
        float speed_mult = 0.6f; // 1.0f = what was in geiss 4.25 and before
        float chromatic_dispersion = 4.0f;  //1.0f = what was in geiss 4.25 and before
        if (iDispBits == 8)
        {
            Solid_Line(floatframe + chaser_offset*speed_mult, VS1);
        }
        /*
        if (iDispBits == 16)
        {
            Solid_Line(floatframe + chaser_offset, VS1);
            Solid_Line(floatframe + chaser_offset + 3.5*(sinf(floatframe*0.03+1)+cosf(floatframe*0.04+3)), VS3);
            Solid_Line(floatframe + chaser_offset - 3.5*(cosf(floatframe*0.05+2)+sinf(floatframe*0.06+4)), VS5);
        }
        else if (iDispBits == 32)
        */
        else
        {
            //snackbar
            Solid_Line(floatframe + chaser_offset*speed_mult, VS1);
            Solid_Line(floatframe + chaser_offset*speed_mult + 3.5*chromatic_dispersion*(sinf(floatframe*0.03+1)+cosf(floatframe*0.04+3)), &VS1[1]);
            Solid_Line(floatframe + chaser_offset*speed_mult - 3.5*chromatic_dispersion*(cosf(floatframe*0.05+2)+sinf(floatframe*0.06+4)), &VS1[2]);
        }
    }
    if (effect[DOTS]==1) One_Dotty_Chaser(floatframe);
    if (effect[NUCLIDE]==1) Nuclide();  //SEE ALSO: RenderDots()


    //if (mode==6) Mode6Edges();
    if (effect[GRID] == 1) Grid();
    
    //DoCrystals(1, gXC + rand()%30 - 15, gYC + rand()%30 - 15 );


    if (effect[SOLAR]==1)
    {
        if (FXW==320)
        {
            fx = 3 + solar_max*(2.4 + 0.35*sinf(intframe*0.05) + 0.4*sinf(intframe*0.038+1));
            Drop_Solar_Particles_320(fx*0.01);
        }
        else
        {
            fx = 3 + solar_max*1.6 + solar_max*0.43*sinf(intframe*0.05) + solar_max*0.43*sinf(intframe*0.038+1);
            Drop_Solar_Particles(fx*0.05);
        }
    }

    Diminish_Center(VS1);
    /*
    if (iDispBits == 16)
    {
        Diminish_Center(VS3);
        Diminish_Center(VS5);
    }
    */
}

void GetWaveData()
{
    float fDiv = (1.0/(64.0 * (640.0/(float)FXW) ));
    float billy = volscale * fDiv;
    #ifdef SAVER
        //added in 4.25:
        billy *= 4.0f;  // because some microphones just can't be cranked up any higher
    #endif
    int i;
    HRESULT hr;


    if (SoundReady && SoundActive)
    {

#if SAVER

        void *pData[2];
        DWORD size[2];
        pData[0] = NULL;
        pData[1] = NULL;
        size[0] = 0;
        size[1] = 0;
        DWORD a, b;
        pDSCB->GetCurrentPosition(&a, &b);    // record pos. (changes always), playback pos. (sometimes sticks)

        //static float s_last_clock;
        //int offset = 44100*4*((clock() - s_last_clock)/(float)CLOCKS_PER_SEC);

        //FILE *infile = fopen("c:\\a.txt", "a");
        //fprintf(infile, "%d %d %p %p\n", size[0], size[1], pData[0], pData[1]);
        //fprintf(infile, "%d %d\n", a, b);
        //fclose(infile);
        
        int offset = 0;
        int new_b = b;

        // if wave input stalled, read further into prev. wave data
        //-----------------------------------------------------------------------------

        //for trigger:
        BUFSIZE             =max(FXW*3, MINBUFSIZE);

        if (b==oldSoundBufPos)
        {
            if (oldSoundBufNum<0) oldSoundBufNum++;
            offset = ((BUFSIZE))*oldSoundBufNum;
        }
        else
        {
            oldSoundBufNum = -3;        // was -2 in first v3.02 release
            oldSoundBufPos = b;
            offset = ((BUFSIZE))*oldSoundBufNum;
        }
        new_b = (b + offset);
        if (new_b < 0) new_b += 176400;
        if (new_b >= 176400) new_b -= 176400;

        // LOCK BUFFER
        hr = pDSCB->Lock(new_b, ((BUFSIZE)), &pData[0], &size[0], &pData[1], &size[1], NULL);
        if (hr != DS_OK)
        {
            dumpmsg("pDSCB->Lock() failed");
            g_QuitASAP = true;
            //exit(880);
        }

        // COPY DATA
        unsigned char *readpos = (unsigned char *)pData[0];
        memcpy(g_SoundBuffer, readpos, size[0]);
        if (size[1] > 0)
        {
            readpos = (unsigned char *)pData[1];
            memcpy(g_SoundBuffer + size[0], readpos, size[1]);
        }

        // UNLOCK BUFFER
        hr = pDSCB->Unlock(pData[0], size[0], pData[1], size[1]);
        if (hr != DS_OK)
        {
            dumpmsg("pDSCB->Unlock() failed");
            g_QuitASAP = true;
            //exit(881);
        }

        // level trigger:
        int v, old_v;
        int FXW_DIV_2 = FXW/2;    // centers the trigger point (rather than @ left)
        for (i=8; i<FXW/2; i+=2)
        {
            old_v = g_SoundBuffer[i + FXW_DIV_2 - 8];
            v     = g_SoundBuffer[i + FXW_DIV_2];
            if ( 
                (abs(    v - last_frame_v    ) <= 256)     // height-match
                &&
                (last_frame_slope*(v-old_v) >= 0)        // slope-match
               )
            {
                last_frame_v = v;
                last_frame_slope = v - old_v;
                break;
            }
        }

        //i = FXW-2;

        if (i < FXW/2)      // trigger.
        {
            //dumpmsg("trigger: frame=%d, pos=%d", intframe, i);
            int iShift = i;
            for (i=iShift; i<BUFSIZE; i++)
            {
                g_SoundBuffer[i-iShift] = g_SoundBuffer[i];
            }
        }
        else    // no trigger
        {
            old_v = g_SoundBuffer[FXW_DIV_2 + 0];
            v     = g_SoundBuffer[FXW_DIV_2 + 8];
            last_frame_v = v;
            last_frame_slope = v - old_v;
        }




        

#endif
#if PLUGIN

        int FXW_DIV_4 = FXW/4;
        int x = 0;
        int y = 5;
        int v;
        int old_v;

        if (visMode != spectrum)
        {
            // --------------- begin level trigger -------------------
            // --------------- begin level trigger -------------------

            y += FXW_DIV_4;                     // start @ middle

            while (y<(511-365+FXW_DIV_4)) // 365 is max index # we'll ever plot (wave #5)        // start @ middle
            {
                old_v = ((int)(g_this_mod->waveformData[0][y-5] ^ 128) - 128);
                v     = ((int)(g_this_mod->waveformData[0][y] ^ 128) - 128);
                if ( 
                    (abs(v - last_frame_v) <= 1)
                    &&
                    (last_frame_slope*(v-old_v) >= 0 )
                   )
                {
                    last_frame_slope = v - old_v;
                    last_frame_v = v;
                    break;
                }
                y++;
            }

            if (y>=(511-365+FXW_DIV_4))       // start @ middle
            {
                y = 5 + FXW_DIV_4;//5;        // start @ middle
                old_v = ((int)(g_this_mod->waveformData[0][y-5] ^ 128) - 128);
                v     = ((int)(g_this_mod->waveformData[0][y] ^ 128) - 128);
                last_frame_slope = v - old_v;
                last_frame_v = v;
            }

            y -= FXW_DIV_4;                     // start @ middle

            // --------------- end of level trigger -------------------
            // --------------- end of level trigger -------------------
        }        


       
        // now fill in g_SoundBuffer[]
        if (visMode == spectrum)
        {
            /*
            for ( ; x<BUFSIZE && y<512; x+=2, y++)
            {
                g_SoundBuffer[x] = ((int)(256-g_this_mod->spectrumData[0][y]) ) << 10;
                g_SoundBuffer[x+1] = ((int)(256-g_this_mod->spectrumData[1][y]) ) << 10;
            }
            */
            int16_t nL, nR;
            int decamt = 2200;//2000;
            for (x=0, y=0; x<BUFSIZE && y<512; x+=2, y++)
            {
                nL = (((int)(256-g_this_mod->spectrumData[0][y])) << 10);
                nR = (((int)(256-g_this_mod->spectrumData[1][y])) << 10);
                nL *= 0.4 + 5.6*(y/512.0);
                nR *= 0.4 + 5.6*(y/512.0);
                g_SoundBuffer[x]   = min(nL, g_SoundBuffer[x]+decamt);
                g_SoundBuffer[x+1] = min(nR, g_SoundBuffer[x+1]+decamt);
            }

            /*
            // set blob_str[]
            int x_max = min(BUFSIZE, 1024);
            int x_lo, x_hi;
            for (int i=0; i<NUM_BLOBS; i++)
            {
                blob_str[i] = 0;
                x_lo = x_max*i/(float)(NUM_BLOBS+1);
                x_hi = x_max*(i+1)/(float)(NUM_BLOBS+1);
                if (x_lo & 1) x_lo--;

                for (x = x_lo; x < x_hi; x+=2)
                    blob_str[i] -= g_SoundBuffer[x];
                blob_str[i] /= (float)(x_hi - x_lo);
                
                // show bars:
                //for (x = x_lo; x < x_hi; x+=2)
                //    g_SoundBuffer[x] = -blob_str[i];
            }

            // move charges
            for (i=0; i<NUM_BLOBS; i++)
            {
                blob_x[i] += 0.2*cosf(intframe*0.0133 + i/(float)NUM_BLOBS*6.28);
                blob_y[i] += 0.2*sinf(intframe*0.0151 + i/(float)NUM_BLOBS*6.28);
            }
            float u, v, d, repulsion;
            for (i=0; i<NUM_BLOBS; i++)
                for (int j=0; j<NUM_BLOBS; j++)
                    if (i != j)
                    {
                        u = blob_x[i] - blob_x[j];
                        v = blob_y[i] - blob_y[j];
                        d = sqrtf(u*u+v*v) + 0.1;
                        repulsion = blob_str[i]*blob_str[j]/d * 0.00000005f;
                        blob_x[i] += u*repulsion;
                        blob_y[i] += v*repulsion;
                    }
            for (i=0; i<NUM_BLOBS; i++)
            {
                blob_x[i] *= 0.98;// - 0.001*blob_str[i]*0.001;
                blob_y[i] *= 0.98;// - 0.001*blob_str[i]*0.001;
            }

            
            dumpmsg("blob_x[2] = %f, blob_str[2] = %f", blob_x[2], blob_str[2]);
            */

            // write matrix - sloppy
            /*
            float dx, dy;
            int j, k;
            for (i=0; i<40; i++)
            for (j=0; j<30; j++)
            {
                matrix[i][j] = 0;
                for (k=0; k<NUM_BLOBS; k++)    
                {
                    dx = i-20 - blob_x[k];
                    dy = j-15 - blob_y[k];
                    d = (dx*dx+dy*dy) + 0.1;
                    matrix[i][j] += blob_str[k] / d;   // actually r^2
                }
            }
            */


        }
        else
        {
            for ( ; x<BUFSIZE && y<512; x+=2, y++)
            {
                g_SoundBuffer[x] = ((int)(g_this_mod->waveformData[0][y] ^ 128) - 128) * 256;
                g_SoundBuffer[x+1] = ((int)(g_this_mod->waveformData[1][y] ^ 128) - 128) * 256;
            }
        }


#endif

        // ------------- do any necessary formatting to sound wave data ---------------

        float center[2];

        // smoothing shows the bass hits more than treble
        for (i=0; i<BUFSIZE-2; i++) g_fSoundBuffer[i] = 0.8*g_SoundBuffer[i] + 0.2*g_SoundBuffer[i+2];
        //for (i=0; i<BUFSIZE-2; i++) g_fSoundBuffer[i] = 0.15*g_SoundBuffer[i] + 0.85*g_SoundBuffer[i+2];

        // now scale sound by volscale & screen-size-scale factor (fDiv)
        for (i=0; i<BUFSIZE; i++) g_fSoundBuffer[i] *= billy;

#if PLUGIN
        if (visMode != spectrum)
#endif
        {
            // get centroid for L, R channels
            center[0] = 0;
            center[1] = 0;
            for (i=0; i<BUFSIZE; i+=8) center[0] += g_fSoundBuffer[i];
            for (i=1; i<BUFSIZE; i+=8) center[1] += g_fSoundBuffer[i];
            center[0] /= (float)FXW*0.125;
            center[1] /= (float)FXW*0.125;

            // now translate wave so center is at zero.
            for (i=0; i<BUFSIZE; i+=2) g_fSoundBuffer[i] -= center[0];
            for (i=1; i<BUFSIZE; i+=2) g_fSoundBuffer[i] -= center[1];
        }


        // ----------------
        // ----------------
        // fourier analysis
        // ----------------
        // ----------------

        //----disable map damping----
        if (iDispBits > 8)
        //----disable map damping----
        {
            int n, i;
            float theta;
            float a, b;    // a + bi
            float old_power;
            float net_power_change = 1.0;
            
            for (n=1; n<FOURIER_DETAIL; n++)
            {
                a = 0;
                b = 0;
                float w = 6.28 * (20*powf(2, n/(float)FOURIER_DETAIL*10)/44100.0);
                for (i=0; i<256; i++)
                {
                    theta = w*i;
                    a += g_fSoundBuffer[i*2] * cosf(theta);
                    b += g_fSoundBuffer[i*2] * sinf(theta);
                }
                //g_power[n] = max(sqrtf(a*a + b*b), g_power[n]*0.95);
                old_power = g_power[n];
                g_power[n] = sqrtf(a*a + b*b);
                g_power_smoothed[n] = g_power_smoothed[n]*0.94 + 0.06*g_power[n];
                net_power_change += fabsf(old_power - g_power[n]);
            }

            // divide the raw sum of 'frequency differences'
            // by the average volume as far back as we know of...
            // this gives us an average freq. change RELATIVE TO
            // the volume of all the other songs that have played.
            net_power_change /= (float)(iVolumeSum / (intframe + 1));
            // at this point, >100 should be full-speed, while toward 0 should be slower.
            // 'suggested_damping' should be from roughly 0.3 ... 1.0
            // -> suggested_damping == 0: no movement
            // -> suggested_damping == 1: full movement
            // -> suggested_damping >  1: full movement (extra ignored)
            net_power_change *= 0.01;
            if (intframe < 50)
                suggested_damping = 1.0;
            else
                suggested_damping = suggested_damping*0.98 + net_power_change*0.02;
            
            // ----disable map damping----
            suggested_damping = 1.0f;
            // ----disable map damping----

            debug_param = suggested_damping;
        }
    }
}

void RenderDots(unsigned char *VS1)
{
    if (SoundReady && SoundActive)// && !SoundEmpty)
    {
        // test for treble hit
        
        int peaks=0, i=0, high, low;
        float vol=0;

        /*
        i=FXW-4;
        while (i>2)
        {
            while (i>2 && g_SoundBuffer[i*2+2] > g_SoundBuffer[i*2])
            {
                i--;
            }
            low = g_SoundBuffer[i*2];
            i--;
            while (i>2 && g_SoundBuffer[i*2+2] < g_SoundBuffer[i*2])
            {
                i--;
            }
            i--;
            high = g_SoundBuffer[i*2];

            peaks++;
            vol += high-low;
        }
        */

        low = g_SoundBuffer[0];
        high = g_SoundBuffer[0];

        i = BUFSIZE - 4;
        while (i>0)
        {
            low = min(low, g_SoundBuffer[i]);
            high = max(high, g_SoundBuffer[i]);
            i -= 4; // was measuring both channels by -= 3 !!!
        }

        /*
        // cancel out vocals for volume calx
        i = BUFSIZE - 4;
        int temp;
        while (i>0)
        {
            temp = abs(g_SoundBuffer[i] - g_SoundBuffer[i+1]);
            low = min(low, temp);
            high = max(high, temp);
            i -= 4;
        }
        */

        vol = (high - low) / 256.0;

        //dumpmsg("%f", vol);

        // saver: low & high are around 10000-20000 (pos. or neg.)
        //        vol is around 80-120


       
        //------------------------------
        // for current_vol:
        //------------------------------
        // plugin:  0/1 = silent
        //          50  = soft
        //          100-150 = normal
        //          200 = loud
        //          255 = max (why?)
        // 
        // saver:   80-120 = normal
        //------------------------------
        //sprintf(szH1, "%d/%d (%5.3f)", low, high, vol); //beely!!!


        //if (peaks>0) vol /= (float)peaks;

        past_vol_pos = (past_vol_pos + 1) % PAST_VOL_N;
        past_vol[past_vol_pos] = current_vol;

        float rate;

        current_vol  = vol;
        //avg_vol_narrow = avg_vol_narrow*0.3 + vol*0.7;
        //avg_vol        = avg_vol*0.85 + vol*0.15;
        //avg_vol_wide   = avg_vol_wide*0.96 + vol*0.04;
        //avg_peaks      = avg_peaks*0.9 + peaks*0.1;
        rate = AdjustRateToFPS(0.30f, 30.0f, fps_at_last_mode_switch); avg_vol_narrow = avg_vol_narrow*rate + vol*(1-rate);
        rate = AdjustRateToFPS(0.85f, 30.0f, fps_at_last_mode_switch); avg_vol        = avg_vol       *rate + vol*(1-rate);
        rate = AdjustRateToFPS(0.96f, 30.0f, fps_at_last_mode_switch); avg_vol_wide   = avg_vol_wide  *rate + vol*(1-rate);
        rate = AdjustRateToFPS(0.90f, 30.0f, fps_at_last_mode_switch); avg_peaks      = avg_peaks     *rate + peaks*(1-rate);
        
        iVolumeSum += (int)avg_vol;



        g_hit = max(g_hit-1, -1);

        #if PLUGIN
            if (!g_bDisableSongTitlePopups && g_hit == -1 && (rand()%7000) < g_random_songtitle_freq*g_random_songtitle_freq*30.0f/fps_at_last_mode_switch) 
            {
                g_hit = g_song_tooltip_frames * fps_at_last_mode_switch/30.0f;
                g_title_R = 128 + rand() % 99;
                g_title_G = 128 + rand() % 99;
                g_title_B = 128 + rand() % 99;
            }
        #endif

        //if (vol > avg_vol*3.5 && vol > avg_vol_wide*3.0) g_hit = 10;
        //if (g_hit == -1 && vol > avg_vol_wide*3.6) g_hit = 5;

        /*
        if (effect[SPECTRAL]==1)//if (mode==17 && !SoundEmpty)
        {
                /////
                static int last_z[NUM_FREQS];
                int i, k;
                int z[NUM_FREQS];

                // problem here!
                if (intframe < 10)
                    for (i=0; i<NUM_FREQS; i++) last_z[i] = 0;

                for (i=0; i<NUM_FREQS; i++) z[i] = 0;
                
                for (k=0; k<NUM_FREQS; k++)
                    for (i=30; i<FREQ_SAMPLES-30; i+=2) z[k] += abs(g_Lsmoothed[k][i]);    

                for (i=0; i<NUM_FREQS; i++) 
                {
                    z[i] = z[i]*0.6 + last_z[i]*0.4;
                    z[i] *= (256.0 - 60.0)/(float)(FREQ_SAMPLES - 60);
                    z[i] *= 0.0005 + 0.00070*(k/(float)(NUM_FREQS-1)) + 0.00045*sqrtf(k/(float)(NUM_FREQS-1));
                    if (z[i] < 0) z[i] = 0;
                    if (z[i] > 255) z[i] = 255;

                    if (min(z[i],255) > last_z[i]-4)
                        g_phase_inc[i] += (last_z[i] - min(z[i],255))*2.5;  // 20 looks cool too

                    last_z[i] = min(z[i],255);
                }


                int nodes = NUM_FREQS;///// + rand()%5;
                int n;
                int phase = 0.8;/////rand()%1000;
                int rad;
                int x,y,cx,cy,str;
                int val;
                int r;// = rand()%8 + 3;  // MAX is **10**

                if (FXW==320)
                    //r = 2 + 20*(vol/avg_vol - 1.25);
                    r = 2 + 40*(vol/avg_vol_narrow - 1.1);
                else
                    //r = 3 + 32*(vol/avg_vol - 1.25);
                    r = 3 + 40*(vol/avg_vol_narrow - 1.1);
                if (r<1) r=1;
                if (FXW==320 && r>7) r=7;
                if (FXW!=320 && r>10) r=10;

                if (FXW==320)
                    rad = 22;///// + rand()%6; //min(35, 14 + 0.7*vol);//(20 + rand()%30);
                else
                    rad = 34;///// + rand()%8; //min(50, 20 + vol);//(20 + rand()%30);

                float cr=1.0, cg=1.0, cb=1.0;
                if (iDispBits>8)
                {
                    int intfram = intframe + chaser_offset;
                    float f = 7*sinf(intfram*0.007+29) + 5*cosf(intfram*0.0057+27);
                    cr = 0.58 + 0.21*sinf(intfram*gF[0] +20-f) + 0.21*cosf(intfram*gF[3] +17+f);
                    cg = 0.58 + 0.21*sinf(intfram*gF[1] +42+f) + 0.21*cosf(intfram*gF[4] +26-f);
                    cb = 0.58 + 0.21*sinf(intfram*gF[2] +57-f) + 0.21*cosf(intfram*gF[5] +35+f);
                }

                int32_t T_offset;
                int pixelsize = (iDispBits==8) ? 1 : 4;

                for (n=0; n<nodes; n++)
                {
                    /////
                    float s = FXW/640.0;
                    float temp_frame = g_phase_inc[n];
                    cx = gXC + s*(64*cosf(temp_frame*0.00451+n*99+3) + 51*cosf(temp_frame*0.00572+n*13+7));  
                    cy = gYC + s*(51*cosf(temp_frame*0.00502+n*78+8) + 45*cosf(temp_frame*0.00653+n*17+5)); 
                    //float crad = ((cx-gXC)*(cx-gXC) + (cy-gYC)*(cy-gYC));
                    //cx = 144.0/(crad+0.1);//max(0, 12 - (crad + 1));
                    //cy =  81.0/(crad+0.1);//max(0,  9 - (crad + 1));
                    /////

                    /////cx = gXC + rad*cosf(n/(float)nodes * 6.28 + phase);
                    /////cy = gYC + rad*sinf(n/(float)nodes * 6.28 + phase);

                    int sqrt_pos = 0;

                    /////
                    //if (n==0) { r = z[n]*0.06 - 2; cr = 0.0; cg = 0.5; cb = 1.0; }
                    //if (n==1) { r = z[n]*0.06 - 2; cr = 1.0; cg = 0.5; cb = 0.0; }  // awesome fiery yellow-red
                    //if (n==2) { r = z[n]*0.06 - 2; cr = 0.2; cg = 1.0; cb = 0.4; }
                    //if (n==3) { r = z[n]*0.06 - 2; cr = 1.0; cg = 0.0; cb = 0.5; }
                    //if (n==4) { r = z[n]*0.06 - 2; cr = 0.5; cg = 1.0; cb = 0.3; }
                    //if (n==5) { r = z[n]*0.06 - 2; cr = 0.2; cg = 0.5; cb = 1.0; }
                    //if (n==6) { r = z[n]*0.06 - 2; cr = 0.5; cg = 0.5; cb = 0.5; }
                    cr = g_DotColor[n][0];
                    cg = g_DotColor[n][1];
                    cb = g_DotColor[n][2];
                    r = z[n]*0.06 - 2;
                    if (r>10) r=10;
                    /////
                   
                    for (y=-10; y<=10; y++)
                    {
                        T_offset = ((cy+y)*FXW + (cx-10)) * pixelsize;
                        for (x=-10; x<=10; x++)
                        {
                            val = (r - sqrt_tab[x+10][y+10])*25;
                            if (val>0)
                            {
                                if (iDispBits==8)
                                {
                                    str = VS1[T_offset] + val;
                                    VS1[T_offset] = min(255, str);
                                }
                                else
                                {
                                    str = VS1[T_offset  ] + val*cb;
                                    VS1[T_offset  ] = min(255, str);
                                    str = VS1[T_offset+1] + val*cg;
                                    VS1[T_offset+1] = min(255, str);
                                    str = VS1[T_offset+2] + val*cr;
                                    VS1[T_offset+2] = min(255, str);
                                }
                            }
                            T_offset += pixelsize;
                        }
                    }
                }
        }
        */

        if (effect[NUCLIDE]>0 && !SoundEmpty)
        {
            //if (vol > avg_vol*1.25)
            if (vol > avg_vol_narrow*1.1)
            {

                int nodes = 3 + rand()%5;
                int n;
                int phase = rand()%1000;
                int rad;
                int x,y,cx,cy,str;
                int val;
                int r;// = rand()%8 + 3;  // MAX is **10**

                if (FXW==320)
                    //r = 2 + 20*(vol/avg_vol - 1.25);
                    r = 2 + 40*(vol/avg_vol_narrow - 1.1);
                else
                    //r = 3 + 32*(vol/avg_vol - 1.25);
                    r = 3 + 40*(vol/avg_vol_narrow - 1.1);
                if (r<1) r=1;
                if (FXW==320 && r>7) r=7;
                if (FXW!=320 && r>10) r=10;

                if (FXW==320)
                    rad = 22 + rand()%6; //min(35, 14 + 0.7*vol);//(20 + rand()%30);
        else {
                    rad = (34 + rand()%8);
          if (FXW > 1024)
            rad = rad * FXW/1024.0f; //min(50, 20 + vol);//(20 + rand()%30);
        }

                float cr=1.0, cg=1.0, cb=1.0;
                if (iDispBits>8)
                {
                    int intfram = intframe + chaser_offset;
                    float f = 7*sinf(intfram*0.007+29) + 5*cosf(intfram*0.0057+27);
                    cr = 0.58 + 0.21*sinf(intfram*gF[0] +20-f) + 0.21*cosf(intfram*gF[3] +17+f);
                    cg = 0.58 + 0.21*sinf(intfram*gF[1] +42+f) + 0.21*cosf(intfram*gF[4] +26-f);
                    cb = 0.58 + 0.21*sinf(intfram*gF[2] +57-f) + 0.21*cosf(intfram*gF[5] +35+f);
                }

                int32_t T_offset;
                int pixelsize = (iDispBits==8) ? 1 : 4;

                for (n=0; n<nodes; n++)
                {
                    cx = gXC + rad*cosf(n/(float)nodes * 6.28 + phase);
                    cy = gYC + rad*sinf(n/(float)nodes * 6.28 + phase);

                    int sqrt_pos = 0;
                   
                    for (y=-10; y<=10; y++)
                    {
                        T_offset = ((cy+y)*FXW + (cx-10)) * pixelsize;
                        for (x=-10; x<=10; x++)
                        {
                            //val = (r - sqrtf(x*x+y*y))*25;
                            val = (r - sqrt_tab[x+10][y+10])*25;
                            if (val>0)
                            {
                                //T_offset = y*FXW + x;
                                if (iDispBits==8)
                                {
                                    str = VS1[T_offset] + val;
                                    VS1[T_offset] = min(255, str);
                                }
                                else
                                {
                                    str = VS1[T_offset  ] + val*cb;
                                    VS1[T_offset  ] = min(255, str);
                                    str = VS1[T_offset+1] + val*cg;
                                    VS1[T_offset+1] = min(255, str);
                                    str = VS1[T_offset+2] + val*cr;
                                    VS1[T_offset+2] = min(255, str);
                                }
                            }
                            T_offset += pixelsize;
                        }
                    }
                }
            }
        }
    }
}

void RenderWave(unsigned char *VS1)
{
    int xL, xR, yL, yR, i, total=0, prev=0;
    float zL, prev_zL, zR, prev_zR;
    int32_t D_offset;
    int fact = 2;//(FXW==320) ? 1 : 2;
    int base = 150;
    
    //dumpmsg("%f", current_vol);

    //base = (current_vol*4.5  + avg_vol * 0.45) - 10;
    #ifdef PLUGIN
        base = (current_vol*4  + avg_vol * 0.4) - 10;
    #endif
    #ifdef SAVER
        //base = (current_vol)/(avg_vol + 0.1f) * 160 * max(0, min(1.0f, (current_vol-0.1f)/0.1f));
        // revised in 4.25:
        base = (current_vol*6 - avg_vol*3.5f)*10 - 40;
    #endif


    //-------------------------------------------

    //------------------------------------
    //ATTEMPT #2: two means, two std. devs
    //------------------------------------

    // find the avg. vol of the past N frames...
    // now draw a line there, and find the average of volumes above it,
    //   and the average of volumes below it.
    // measure avg_dev_lo and avg_dev_hi... if both low, we're in beat mode.

    // cheat: store a smoothed-out volume tape, not a jaggy one
    past_vol[past_vol_pos] = avg_vol_narrow;

    // 1. get avg vol
    float avg_vol_uniform = 0;
    for (i=0; i<PAST_VOL_N; i++) avg_vol_uniform += past_vol[i];
    avg_vol_uniform /= (float)PAST_VOL_N;

/*  // 2. get average hi-vol and average lo-vol
    float avg_vol_lo = 0;
    float avg_vol_hi = 0;
    int   n_lo = 0;
    int   n_hi = 0;
    for (i=0; i<PAST_VOL_N; i++) 
    {
        if (past_vol[i] > avg_vol_uniform)
        {
            avg_vol_hi += past_vol[i];
            n_hi++;
        }
        else
        {
            avg_vol_lo += past_vol[i];
            n_lo++;
        }
    }
    avg_vol_hi /= (float)n_hi;
    avg_vol_lo /= (float)n_lo;

    // 3. get average deviation from each of the volumes.
    float avg_dev_lo = 0;
    float avg_dev_hi = 0;
    for (i=0; i<PAST_VOL_N; i++) 
        if (past_vol[i] > avg_vol_uniform)
            avg_dev_hi += fabs(past_vol[i] - avg_vol_hi);
        else
            avg_dev_lo += fabs(past_vol[i] - avg_vol_lo);
    avg_dev_hi /= (float)n_hi;
    avg_dev_lo /= (float)n_lo;
*/

    // BEATMODE:
    // to know whether or not we should be in beat mode,
    // look at the n past 'avg_vol_narrow' values.  This
    // creates a smoothed wave that is flattish under normal
    // circumstances, but very evenly bumpy (w/high amplitude)
    // when a good beat is on.


    //bBeatMode = false;
    float beat_strength = 0;
    for (i=1; i<PAST_VOL_N; i++) 
        beat_strength += max(0, fabs(past_vol[i] - past_vol[i-1]) - avg_vol_uniform*0.15);
        //beat_strength += max(0, max(0, past_vol[i] - past_vol[i-1]) - avg_vol_uniform*0.15);
    beat_strength /= avg_vol_uniform;
    beat_strength *= 10;
    if (avg_vol_uniform < 10) beat_strength = 0;

    if (beat_strength > 90 + 19) bBeatMode = true;
    if (beat_strength < 90 - 19) bBeatMode = false;


/*
    // Code to visualize the past n 'avg_vol_narrow' values:
    memset((void *)g_fSoundBuffer, 0, BUFSIZE*sizeof(float));
    for (i=0; i<PAST_VOL_N; i++)
    {
        int j = i - past_vol_pos;
        if (j<0) j += PAST_VOL_N;
        
        g_fSoundBuffer[j*2] = past_vol[i]/4;
    }
*/


    // NOW we know bBeatMode.  Use old code to get beat-mode brightness.


    
    //float avg_vol_uniform = 0;
    //for (i=0; i<PAST_VOL_N; i++) avg_vol_uniform += past_vol[i];
    //avg_vol_uniform /= (float)PAST_VOL_N;
    float variance = 0;
    float temp;
    for (i=0; i<PAST_VOL_N; i++) 
    {
        temp = (past_vol[i] - avg_vol_uniform);
        variance += temp*temp;
    }
    variance /= (float)(PAST_VOL_N - 1);
    float std_dev = sqrtf(variance);

    /*
    float avg_vol_change = 0;
    for (i=1; i<PAST_VOL_N; i++) avg_vol_change += abs(past_vol[i] - past_vol[i-1]);
    avg_vol_change /= (float)(PAST_VOL_N - 1);
    float avg_vol_change_var = 0;
    for (i=1; i<PAST_VOL_N; i++) 
    {
        temp = abs(past_vol[i] - past_vol[i-1]) - avg_vol_change;
        avg_vol_change_var += temp*temp;
    }
    avg_vol_change_var /= (float)(PAST_VOL_N - 2);
    float avg_vol_change_std_dev = sqrtf(avg_vol_change_var);
    */

    float max_vol = 0;
    for (i=0; i<PAST_VOL_N/3; i++) max_vol = max(max_vol, past_vol[i]);
    if (avg_vol_narrow > max_vol*fBigBeatThreshold)//avg_vol_narrow - avg_vol_wide > avg_vol_change_std_dev*2)
        bBigBeat = true;
    else
        bBigBeat = false;

    //damped_std_dev = damped_std_dev*0.95 + std_dev*0.05;
    //if (damped_std_dev > 12.7f) bBeatMode = true;
    //if (damped_std_dev < 11.3f) bBeatMode = false;

    #ifdef PLUGIN
        float brite_scale = (current_vol - avg_vol_uniform) / (std_dev*0.5);
        //float brite_scale = (current_vol - avg_vol_uniform*0.8f) / (avg_vol_uniform*0.95f - avg_vol_uniform*0.8f);
    #endif
    #ifdef SAVER
        // revised in 4.25:
        float brite_scale = 1.0f;
    #endif

    // at current_vol:         val = 0
    // at 0.5 std. dev. above: val = 1
    if (brite_scale < 0.0f) brite_scale = 0.0f;
    if (brite_scale > 1.0f) brite_scale = 1.0f;

    if (bBeatMode && 
        g_bUseBeatDetection &&
        #if PLUGIN
        visMode != spectrum &&
        #endif
        waveform != 6
        ) 
    {
        // use beat detection mode
        // a "beat" hits whenever current_vol is > a user-definable # of standard 
        //   deviations away from the uniform average vol. over the last so many 
        //   frames.  Usually 0.5 standard deviations.

        base *= brite_scale;
    }

    // shifting
    int old_slider1 = slider1;
    //if (bBeatMode && brite_scale >= 0.99f && g_bSlideShift && g_FramesSinceShift > g_SlideShiftMinFrames)
    // new technique:
    if (bBeatMode && g_bSlideShift)
    {
        //static float last_peak = 0;

        if (current_vol > g_ShiftMaxVol)
        {
            g_ShiftMaxVol = current_vol * 1.05;
            //last_peak = g_ShiftMaxVol;

            if (g_FramesSinceShift > 2)
            {
                g_FramesSinceShift = 0;

                //slider1 = (rand()%FXW - FXW/2)/115;//80;
                slider1 = ((rand()%(FXW/2)) + 50)/145;//80;

                // force it to go the other way on X this time:
                if (old_slider1 > 0) slider1 *= -1;

                // vertical shift: -1, 0, or 1 allowed
                slider1 += (rand()%3-1)*FXW;
            }
        }
        else
        {
            float rate = 0.975f;
            rate = AdjustRateToFPS(0.975f, 30.0f, fps);
            float limit_vol = avg_vol_uniform*1.43;
            g_ShiftMaxVol = g_ShiftMaxVol*rate + limit_vol*(1-rate);
            g_FramesSinceShift++;

            //if (g_ShiftMaxVol > limit_vol)
            //{
                // within 50 frames, it will go back to low expectations.
                //g_ShiftMaxVol -= (last_peak - limit_vol)*0.02;
            //}
        }
    }



    sprintf(szDEBUG, "%5.3f ", debug_param);
    if (bBeatMode) strcat(szDEBUG, ", beat mode");
    if (bBigBeat) strcat(szDEBUG, ", BIGBEAT");
        

    ///if (iDispBits==8)
    ///    base += wave_brightness;

    if (base>155) base = 155;
    if (base<0) base = 0;
    
#if SAVER
    // set SoundEmpty
    if (base==0)
    {
        frames_since_silence++;
        if (frames_since_silence > frames_til_auto_switch*2)
        {
            SoundEmpty = true;
        }
    }
    else
    {
        frames_since_silence=0;
        SoundEmpty = false;
    }
#endif    

    //float fDiv = 1.0/(256.0*(640.0/(float)FXW));
    //float fDiv = (1.0/(128.0*(320.0/(float)FXW)));

    unsigned char r, g, b;
    r = base;
    g = base;
    b = base;
    if (iDispBits>8)
    {
        if (g_bSyncColorToSound)
        {
            // 1. base color on time
            float ir2, ig2, ib2;   // was -int-
            int intfram = intframe + chaser_offset;
            float f = 7*sinf(intfram*0.006+59) + 5*cosf(intfram*0.0077+17);
            ir2 = base*1.07 * (1 + 0.3*sinf(intfram*gF[0] +10-f)) * (1 + 0.20*cosf(intfram*gF[1] +37+f));
            ig2 = base*1.07 * (1 + 0.3*sinf(intfram*gF[2] +32+f)) * (1 + 0.20*cosf(intfram*gF[3] +16-f));
            ib2 = base*1.07 * (1 + 0.3*sinf(intfram*gF[4] +87-f)) * (1 + 0.20*cosf(intfram*gF[5] +25+f));
            
            ///ir += wave_brightness;
            ///ig += wave_brightness;
            ///ib += wave_brightness;

            // 2. base color on frequency
            int a;
            float ir=0, ig=0, ib=0;
            //4.20:
            //for (a=0; a<FOURIER_DETAIL*0.34; a++) ir += g_power_smoothed[a];
            //for (a=FOURIER_DETAIL*0.36; a<FOURIER_DETAIL*0.63; a++) ig += g_power_smoothed[a];
            //for (a=FOURIER_DETAIL*0.6; a<FOURIER_DETAIL; a++) ib += g_power_smoothed[a];
            for (a=0; a<FOURIER_DETAIL*0.31; a++) ir += g_power_smoothed[a];
            for (a=FOURIER_DETAIL*0.30; a<FOURIER_DETAIL*0.59; a++) ig += g_power_smoothed[a];
            for (a=FOURIER_DETAIL*0.56; a<FOURIER_DETAIL; a++) ib += g_power_smoothed[a];
            ir *= 0.93;
            ig *= 1.18;
            ib *= 2.40;
             ir -= ib*0.4;
            f = base/sqrtf(ir*ir + ig*ig + ib*ib);
            ir *= f;
            ig *= f;
            ib *= f;

            // 3. combine
            ir = (ir*0.97 + ir2*0.03)*1.35;
            ig = (ig*0.97 + ig2*0.03)*1.35;
            ib = (ib*0.97 + ib2*0.03)*1.35;

            if (ir<0) ir=0;
            if (ir>255) ir=255;
            if (ig<0) ig=0;
            if (ig>255) ig=255;
            if (ib<0) ib=0;
            if (ib>255) ib=255;
            r = ir;
            g = ig;
            b = ib;
        }
        else
        {
            float ir, ig, ib;   // was -int-
            float intfram = (float)(intframe + chaser_offset) * 30.0f/fps_at_last_mode_switch;
            float f = 7*sinf(intfram*0.006+59) + 5*cosf(intfram*0.0077+17);
            float c1 = 0.55f;  // was 0.3f in v4.25 and before
            float c2 = 0.50f;  // was 0.2f in v4.25 and before
            ir = base*1.07 * (1 + c1*sinf(intfram*gF[0] +10-f)) * (1 + c2*cosf(intfram*gF[1] +37+f));
            ig = base*1.07 * (1 + c1*sinf(intfram*gF[2] +32+f)) * (1 + c2*cosf(intfram*gF[3] +16-f));
            ib = base*1.07 * (1 + c1*sinf(intfram*gF[4] +87-f)) * (1 + c2*cosf(intfram*gF[5] +25+f));
                
            ///ir += wave_brightness;
            ///ig += wave_brightness;
            ///ib += wave_brightness;

            // COFFEE FILTER:
            //ig = powf(ir/255.0f, 1.5f)*255.0f;
            //ib = powf(ir/255.0f, 1.9f)*255.0f;
            /*
            float ig2, ib2, ir2;
            float fExp, fLerp;
            #define FRAND ((rand()%4716)/4715.0f);
            #define GEN_RAND(x,y,z) {   \
                fExp = 0.5f + 0.5f*cosf(intframe*(x+0.432f)+2);   \
                fExp = powf(fExp, 0.5f); \
                if (z==1) { \
                    fLerp = -9.0f + 11.0f*cosf(intframe*x+y+2);   \
                    fLerp = max(0,min(1,fLerp));   \
                } \
            }
            GEN_RAND(0.27929f,1,1); fLerp=0; ig2 = powf(ir/255.0f, 1+fExp)*255.0f*fLerp + ig*(1-fLerp);
            GEN_RAND(0.24552f,3,0); fLerp=0; ib2 = powf(ir/255.0f, 1+fExp)*255.0f*fLerp + ib*(1-fLerp);
            GEN_RAND(0.23016f,7,1); fLerp=1; ir2 = powf(ig/255.0f, 1+fExp)*255.0f*fLerp + ir*(1-fLerp);
            GEN_RAND(0.25357f,6,0); fLerp=1; ib2 = powf(ig/255.0f, 1+fExp)*255.0f*fLerp + ib*(1-fLerp);
            GEN_RAND(0.24987f,2,1); fLerp=0; ir2 = powf(ib/255.0f, 1+fExp)*255.0f*fLerp + ir*(1-fLerp);
            GEN_RAND(0.22934f,0,0); fLerp=0; ig2 = powf(ib/255.0f, 1+fExp)*255.0f*fLerp + ig*(1-fLerp);
            ir = ir2;
            ig = ig2;
            ib = ib2;
            */

            /*
            float desat = -14.0f + 6.0f*cosf(intframe/fps_at_last_mode_switch*0.63723f+2) 
                                 + 8.0f*cosf(intframe/fps_at_last_mode_switch*0.58399f+1)
                                    + 7.0f*cosf(intframe/fps_at_last_mode_switch*0.43818f+4);
            desat = min(1,max(0,desat));
            float lum = ir*0.32f + ig*0.43f + ib*0.25f;
            ir = lum*desat + ir*(1-desat);
            ig = lum*desat + ig*(1-desat);
            ib = lum*desat + ib*(1-desat);
            */

            if (ir<0) ir=0;
            if (ir>255) ir=255;
            if (ig<0) ig=0;
            if (ig>255) ig=255;
            if (ib<0) ib=0;
            if (ib>255) ib=255;
            r = ir;
            g = ig;
            b = ib;
        }    
    }

        //UNFIN
        // NEW_COLORS_430
        // ALTERNATE WAVE COLORING METHOD:
        #if 0
        {
            #define FRAND ((rand()%4716)/4715.0f)
            #define COIN_TOSS (FRAND < 0.5f)
            static float sc[3] = { FRAND, FRAND, FRAND };
            static float dc[3] = { FRAND, FRAND, FRAND };
            static int start_frame = 0;
            static int lerp_frames = 60;
            if (intframe >= start_frame + lerp_frames) 
            {
                sc[0] = dc[0];
                sc[1] = dc[1];
                sc[2] = dc[2];
                dc[0] = powf(FRAND, 1.0f)*1.2f;
                dc[1] = powf(FRAND, 1.0f)*1.2f;
                dc[2] = powf(FRAND, 1.0f)*1.2f;
                float desat = powf(FRAND, 3.0f);
                float lum = 0.3f*dc[0] + 0.5f*dc[1] + 0.2f*dc[2];
                dc[0] = dc[0]*(1.0f-desat) + lum*desat;
                dc[1] = dc[1]*(1.0f-desat) + lum*desat;
                dc[2] = dc[2]*(1.0f-desat) + lum*desat;
                
                /*
                if (FRAND < 0.25f)
                  dc[0] = powf(  COIN_TOSS ? dc[1] : dc[2], 0.5f + 0.5f*FRAND );
                else if (FRAND < 0.333f)
                  dc[1] = powf(  COIN_TOSS ? dc[0] : dc[2], 0.5f + 0.5f*FRAND );
                else if (FRAND < 0.5f)
                  dc[2] = powf(  COIN_TOSS ? dc[1] : dc[0], 0.5f + 0.5f*FRAND );
                  */
                start_frame = intframe;
                float lerp_duration_seconds = 0.5f * powf(1.6f, FRAND*2-1) * 1.1f;
                lerp_frames = lerp_duration_seconds * fps_at_last_mode_switch;
            }
            float t = (intframe - start_frame) / (float)lerp_frames;
            int ir = (sc[0]*(1.0f-t) + dc[0]*t)*255;
            int ig = (sc[1]*(1.0f-t) + dc[1]*t)*255;
            int ib = (sc[2]*(1.0f-t) + dc[2]*t)*255;
            


            if (ir<  0) ir=0;
            if (ir>255) ir=255;
            if (ig<  0) ig=0;
            if (ig>255) ig=255;
            if (ib<  0) ib=0;
            if (ib>255) ib=255;
            r = ir;
            g = ig;
            b = ib;
        }
        #endif



    //EXPERIMENT
    //EXPERIMENT
    //EXPERIMENT
    /*
    if ((intframe % 50)==0)
    {
        dumpmsg("sound: wave#/ready/active/empty = %d/%d/%d/%d", waveform, SoundReady, SoundActive, SoundEmpty);
    }
    */
    //EXPERIMENT
    //EXPERIMENT
    //EXPERIMENT

    if (SoundReady && SoundActive && !SoundEmpty)
    {
        //EXPERIMENT
        //EXPERIMENT
        //EXPERIMENT
        /*
        if ((intframe % 50)==0)
        {
            dumpmsg("wave: r=%d g=%d b=%d", r, g, b);
        }
        r = 100;
        g = 120;
        b = 140;
        */
        //EXPERIMENT
        //EXPERIMENT
        //EXPERIMENT

        // for high resolutions, add more sound samples (via interpolation)
        int passes = 0;
        if (waveform==1 || waveform==2)
        {
            if (FXW >= 1920)
                passes = 2;
            else if (FXW > 1024)
                passes = 1;
        }
        else 
        {
            if (FXW >= 1440)
                passes = 1;
        }
        float amp_scale_per_pass = 1.14f;
        for (int pass=0; pass<passes; pass++)
        {
            float fSoundBufTemp[16384];
            memcpy(fSoundBufTemp, g_fSoundBuffer, sizeof(float) * min(16384,FXW*4));
            float sL = fSoundBufTemp[0];
            float sR = fSoundBufTemp[1];
            for (int i=0; i<FXW*2; i+=2)
            {
                float tL = fSoundBufTemp[i+2] * amp_scale_per_pass;
                float tR = fSoundBufTemp[i+3] * amp_scale_per_pass;
                float* p = &g_fSoundBuffer[i*2];
                p[0] = sL;
                p[1] = sR;
                p[2] = 0.5f*(sL + tL);
                p[3] = 0.5f*(sR + tR);
                sL = tL;
                sR = tR;
            }
        }


        if (waveform==1)
        {
            int y_center = gYC;//FXH/2;
            int start = 0;
            int end = FXW;
    
            if (mode==10) 
            {
                y_center = ((FXH - FX_YCUT) + (FXH*0.5)) * 0.5;
                #if PLUGIN
                    if (visMode == spectrum)
                    {
                        y_center = 0.9*(FXH - FX_YCUT) + 0.1*(FXH*0.5);
                    }
                #endif
                start += 10;
                end -= 10;
                if (FXW >= 640)
                {
                    start += 5;
                    end -= 5;
                }
            }
            //zL = g_fSoundBuffer[0] + y_center;
            zL = g_fSoundBuffer[start & 0xFFFFFFFE] + y_center;
            
            for (i=start; i<end; i++)  
            {
                prev_zL = zL;
                zL = g_fSoundBuffer[(i & 0xFFFFFFFE)] + y_center;
                zL = prev_zL*0.9 + zL*0.1;
                yL = (int)zL;

                if (yL >= FX_YCUT_HIDE && yL < FXH-FX_YCUT_HIDE)
                {
                    D_offset = FXW*yL + i;
                    if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                    else 
                    { 
                        D_offset *= 4; 
                        if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                        if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                        if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                    }                
                }
            }
        }
        else if (waveform==2)
        {
            float fDiv = 0.7;
            float h1 = gYC - FXH*0.12;//FXH*0.4;
            float h2 = gYC + FXH*0.12;//FXH*0.6;
            zL = g_fSoundBuffer[0] * fDiv + h1;
            zR = g_fSoundBuffer[1] * fDiv + h2;
            for (i=0; i<FXW; i++)  
            {
                prev_zL = zL;
                prev_zR = zR;
                zL = g_fSoundBuffer[(i & 0xFFFFFFFE)] * fDiv + h1;
                zR = g_fSoundBuffer[(i & 0xFFFFFFFE)+1] * fDiv + h2;
                zL = prev_zL*0.9 + zL*0.1;
                zR = prev_zR*0.9 + zR*0.1;
                yL = (int)zL;
                yR = (int)zR;
                if (yL>FX_YCUT_HIDE && yL<FXH-FX_YCUT_HIDE) 
                {
                    D_offset = FXW*yL + i;
                    if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                    else 
                    { 
                        D_offset *= 4; 
                        if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                        if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                        if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                    }
                }
                if (yR>FX_YCUT_HIDE && yR<FXH-FX_YCUT_HIDE) 
                {
                    D_offset = FXW*yR + i;
                    if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                    else 
                    { 
                        D_offset *= 4; 
                        if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                        if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                        if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                    }
                }
            }
        }
        else if (waveform==3)
        {
            zL = g_fSoundBuffer[FX_YCUT_HIDE^(FX_YCUT_HIDE & 1)] + gXC;//FXW/2;
            for (i=FX_YCUT_HIDE; i<FXH-FX_YCUT_HIDE; i++)  
            {
                prev_zL = zL;
                zL = g_fSoundBuffer[(i & 0xFFFFFFFE)] + gXC;//FXW/2;
                zL = prev_zL*0.9 + zL*0.1;
                xL = (int)zL;
                if (xL>=0 && xL<FXW)
                {
                    D_offset = FXW*i + xL;
                    if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                    else 
                    { 
                        D_offset *= 4; 
                        if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                        if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                        if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                    }
                }
            }
        }
        else if (waveform==4)
        {
            float fDiv = 0.9;
            zL = g_fSoundBuffer[FX_YCUT_HIDE^(FX_YCUT_HIDE & 1)] * fDiv;
            zR = g_fSoundBuffer[FX_YCUT_HIDE^(FX_YCUT_HIDE & 1)+1] * fDiv;
            for (i=FX_YCUT_HIDE; i<FXH-FX_YCUT_HIDE; i++)  
            {
                prev_zL = zL;
                prev_zR = zR;
                zL = g_fSoundBuffer[(i & 0xFFFFFFFE)] * fDiv;
                zR = g_fSoundBuffer[(i & 0xFFFFFFFE)+1] * fDiv;
                zL = prev_zL*0.9 + zL*0.1;
                zR = prev_zR*0.9 + zR*0.1;
                xL = (int)zL + i;
                xR = (int)zR + i + (FXW-FXH);
                if (xL>=0 && xL<FXW)
                {
                    D_offset = FXW*i + xL;
                    if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                    else 
                    { 
                        D_offset *= 4; 
                        if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                        if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                        if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                    }
                }
                if (xR>=0 && xR<FXW)
                {
                    D_offset = FXW*i + xR;
                    if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                    else 
                    { 
                        D_offset *= 4; 
                        if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                        if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                        if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                    }
                }
            }
        }
        else if (waveform==5)
        {
            int px, py;
            float range_inv = 1.0/WAVE_5_BLEND_RANGE;
            float amt;
            float rad;
            float base_rad = (FXW==320) ? 40.0 : FXW/640.0f*60.0;
            float fDiv = 0.7;

            for (i=0; i<WAVE_5_BLEND_RANGE; i++)  
            {
                amt = i * range_inv;
                g_fSoundBuffer[(i & 0xFFFFFFFE)] = g_fSoundBuffer[(i & 0xFFFFFFFE)]*(amt) + (1-amt)*(g_fSoundBuffer[(i+314)^((i+314)&1)]);
            }
                        
            rad = base_rad + g_fSoundBuffer[0] * fDiv;
            for (i=0; i<314; i++)  
            {
                rad = rad*0.5 + 0.5*(base_rad + g_fSoundBuffer[(i & 0xFFFFFFFE)] * fDiv);
                if (rad>=5)
                {
                    px = (float)gXC + rad*cosf(i*0.02);
                    py = (float)gYC + rad*sinf(i*0.02);
                    if (px>=0 && px<FXW && py>=FX_YCUT && py<FXH-FX_YCUT)
                    {
                        D_offset = FXW*py + px;
                        if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                        else 
                        { 
                            D_offset *= 4; 
                            if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                            if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                            if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                        }
                    }
                }
            }
        }
        else if (waveform==6)
        {
            int px, py;
            float fDiv = 1.2;
                        
            float px2, py2;//, ang;
            float ang = sinf(intframe*0.01);
            float cosang = cosf(ang);
            float sinang = sinf(ang);

            px2 = g_fSoundBuffer[0];
            py2 = g_fSoundBuffer[1];
            for (i=0; i<314; i++)  
            {
                px2 = px2*0.5 + 0.5*g_fSoundBuffer[i*2]*fDiv;
                py2 = py2*0.5 + 0.5*g_fSoundBuffer[i*2+1]*fDiv;
                px = px2*cosang + py2*sinang;
                py = px2*-sinang + py2*cosang;
                px += gXC;
                py += gYC;
                if (px>=0 && px<FXW && py>=FX_YCUT && py<FXH-FX_YCUT)
                {
                    D_offset = FXW*py + px;
                    if (iDispBits==8) { if (VS1[D_offset]<r) VS1[D_offset] = r; }
                    else 
                    { 
                        D_offset *= 4; 
                        if (VS1[D_offset  ]<b) VS1[D_offset  ] = b; 
                        if (VS1[D_offset+1]<g) VS1[D_offset+1] = g; 
                        if (VS1[D_offset+2]<r) VS1[D_offset+2] = r; 
                    }
                }
            }
        }
        else if (waveform==7)
        {
            /*
            float d2, dx, dy, val;
            for (int k=0; k<NUM_BLOBS; k++)    
            {
                unsigned char c1 = 50;        //b
                unsigned char c2 = 150*((k+1)/(float)NUM_BLOBS);      //g
                unsigned char c3 = 150;//150-c2;    //r

                float x_halfspan = 10;
                float y_halfspan = x_halfspan;
                int x2, y2;
                for (int x = blob_x[k]-x_halfspan; x < blob_x[k]+x_halfspan; x++)
                for (int y = blob_y[k]-y_halfspan; y < blob_y[k]+y_halfspan; y++)
                {
                    dx = x - blob_x[k];
                    dy = y - blob_y[k];
                    d2 = (dx*dx + dy*dy) / (blob_str[k]*0.001);
                    d2 *= 0.03;

                    if (d2 < 0.5)     // if (d < 0.707)
                    {
                        val = 128;//4*(0.25 + d2*d2 - d2) * 255;
                        x2 = x + gXC;
                        y2 = y + gYC;
                    
                        if (iDispBits==8) 
                        {
                            if (x2>=0 && x2<FXW && y2>=FX_YCUT && y2<FXH-FX_YCUT)
                            {
                                D_offset = y2*FXW + x2;
                                if (VS1[D_offset] < val) VS1[D_offset] = val; 
                            }
                        }
                        else
                        {
                            if (x2>=0 && x2<FXW && y2>=FX_YCUT && y2<FXH-FX_YCUT)
                            {
                                D_offset = (y2*FXW + x2) << 2;
                                if (VS1[D_offset]   < c1) VS1[D_offset  ] = c1;
                                if (VS1[D_offset+1] < c2) VS1[D_offset+1] = c2;
                                if (VS1[D_offset+2] < c3) VS1[D_offset+2] = c3;
                            }
                        }

                    }
                }
            }
            */

            // the rotating line...

            float ang = avg_vol_narrow*0.03;
            float dx = sinf(ang);
            float dy = cosf(ang);
            int x, y;
            float t;

            dx = cosf(intframe*0.03);
            dy = sinf(intframe*0.03);

            if (fabs(dx) > 0.001)    
            {
                float m = dy/dx;
                if (fabs(dx) > fabs(dy))
                {
                    float b = gYC - m*gXC;
                    for (x=0; x<FXW; x++)
                    {
                        y = (int)(m*x + b);
                        if (y>FX_YCUT && y < FXH-FX_YCUT)
                        {
                            D_offset = FXW*y + x;
                            t = min(1.0, fabs(g_fSoundBuffer[x^(x&1)]/64.0f));
                            if (iDispBits==8) { if (VS1[D_offset]<r*t) VS1[D_offset] = r*t; }
                            else 
                            { 
                                D_offset *= 4; 
                                if (VS1[D_offset  ]<r*t) VS1[D_offset  ] = b*t; 
                                if (VS1[D_offset+1]<g*t) VS1[D_offset+1] = g*t; 
                                if (VS1[D_offset+2]<b*t) VS1[D_offset+2] = r*t; 
                            }
                        }
                    }
                }
                else
                {
                    m = dx/dy;
                    float b = gXC - m*gYC;
                    for (y=FX_YCUT; y<FXH-FX_YCUT; y++)
                    {
                        x = (int)(m*y + b);
                        if (x>=0 && x<FXW)
                        {
                            D_offset = FXW*y + x;
                            t = min(1.0, fabs(g_fSoundBuffer[y^(y&1)]/64.0f));
                            if (iDispBits==8) { if (VS1[D_offset]<r*t) VS1[D_offset] = r*t; }
                            else 
                            { 
                                D_offset *= 4; 
                                if (VS1[D_offset  ]<r*t) VS1[D_offset  ] = b*t; 
                                if (VS1[D_offset+1]<g*t) VS1[D_offset+1] = g*t; 
                                if (VS1[D_offset+2]<b*t) VS1[D_offset+2] = r*t; 
                            }
                        }
                    }
                }
            }
        }
    }
}


// Portable restoration of the bilinear warp in Geiss 4.30 (BSD-3-Clause).

#include <cstdint>
#include <cstring>
void Process_Map(void* source, void* destination) {
    const auto started = clock();
    auto src = static_cast<const uint8_t*>(source);
    auto dst = static_cast<uint8_t*>(destination);
    ptrdiff_t offset = 0;
    const int stride = FXW * (iDispBits == 8 ? 1 : 4);
    const int channels = iDispBits == 8 ? 1 : 3;
    const int step = iDispBits == 8 ? 1 : 4;
    const ptrdiff_t length = ptrdiff_t(stride) * FXH;
    for (int n = 0; n < FX_YCUT_NUM_LINES; ++n) {
        const auto weights = DATA_FX + FX_YCUT_xFXW_x8 + ptrdiff_t(n) * 8;
        int32_t delta;
        memcpy(&delta, weights + 4, sizeof(delta));
        offset += delta;
        const auto out = ptrdiff_t(FX_YCUT_xFXW + n) * step;
        if (offset < 0 || offset + stride + step + channels > length) {
            memset(dst + out, 0, step);
            continue;
        }
        for (int c = 0; c < channels; ++c) {
            dst[out+c] = uint8_t((src[offset+c]*weights[0] + src[offset+step+c]*weights[1]
                + src[offset+stride+c]*weights[2] + src[offset+stride+step+c]*weights[3]) >> 8);
        }
        if (step == 4) dst[out+3] = 255;
    }
    core_clock_time += clock() - started;
}

#include "engine_adapter.inc"
