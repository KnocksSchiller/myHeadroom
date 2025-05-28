#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxJSON.h"
#include "ofxImGui.h"
#include "ofxFastFboReader.h"
#include "ofxFFmpegRecorder.h"
#include "softcam.h"

class ofApp : public ofBaseApp {

public:
	void setup();
	void update();
	void draw();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseEntered(int x, int y);
	void mouseExited(int x, int y);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);
	void gotMessage(ofMessage msg);

	// Aufraeum-Funktion
	void exit();

	// Enums fuer Video- und Audioeffekte, um die spaeter einfacher zu uebernehmen
	enum mhVfx {
		Recolor, Schwarz_Weiss, Chroma_Key, Mosaic_Circles, Blur, Negative, FrameRate, Mirror, Ascii, Canny_Edges, VFXSIZE
	};

	enum mhAfx {
		pitchShift, equalizer, delay, distort, robot, AFXSIZE
	};

	// Kamera
	ofVideoGrabber camera;
	float camWidth;
	float camHeight;
	float camPosX;
	float camPosY;
	float aspectRatio;
	void initCamera();
	std::vector<ofVideoDevice> videoDevices;

	// GUI
	ofxImGui::Gui gui;
	bool showSettings;
	void listVfxs();
	void listAfxs();
	void listVideos(int wWidth);
	std::string shortenFileName(const std::string& fileName, size_t limit);

	// Record
	ofFbo recordFbo;
	ofxFastFboReader fastFbo;
	ofxFFmpegRecorder recorder;
	ofPixels recordPixel;
	bool isRecordingVideo;
	bool isRecordingAudio;

	// Video
	bool videoIsLooping;
	bool videoLoopsBAF;
	std::string currentVideo;
	ofVideoPlayer videoPlayer;
	bool videoIsPlaying;
	float videoVolume;
	float videoSpeed;
	ofPixels videoPixel;
	ofPixels tempPixel;
	ofTexture outTexture;
	void startStopVideoPlayback();
	void startStopStutter();

	// Vfx
	std::vector<bool> vfxs;
	bool activeBNW;
	bool activeBlur;
	bool activeNegative;
	bool activeMirror;
	bool activeRecolor;
	bool activeChroma;
	bool activeMosaicCircle;
	bool activeCanny;
	bool activeFrameRate;
	bool activeAscii;
	void applyVfxs();
	int vfx_recolor_value;
	int vfx_recolor_brightness;
	int vfx_recolor_saturation;
	int vfx_recolor_alpha;
	int vfx_blur_type;
	int vfx_blur_intensity;
	bool vfx_mirror_v;
	bool vfx_mirror_h;
	int vfx_maxFrames;
	int vfx_currentFrames;
	int vfx_ascii_fontsize;
	int vfx_canny_th1;
	int vfx_canny_th2;
	int vfx_canny_dilation;
	int vfx_canny_resize;

	// stutter
	bool stutter_enabled;
	bool activeStutter;
	float stutterTime;
	std::vector<ofPixels> vfx_stutterBuffer;
	std::vector<float> afx_stutterBuffer;
	int vfx_stutter_writePoint, afx_stutter_writePoint;
	int vfx_stutter_readPoint, afx_stutter_readPoint;
	int vfx_stutter_readPointStart, afx_stutter_readPointStart;
	int getReadPoint(int writePoint, float time, int factor, int size);

	// Audio
	void initSoundStream();
	size_t lastAudioTimeReset;
	int bufferCounter;
	float audioFPS;
	int audioCounter;
	int audioInId;
	int audioOutId;
	void audioIn(ofSoundBuffer& input);
	void audioOut(ofSoundBuffer& output);
	std::vector<float> inputBuffer;
	std::vector<ofSoundDevice> audioDevices;
	void checkIOId(int id, bool isInput = true);
	ofSoundStreamSettings soundSettings;
	ofSoundStream soundStream;
	ofPolyline waveform;
	float outSampleL;
	float outSampleR;
	float delaySample;
	ofSoundPlayer soundPlayer;
	std::string currentSound;
	std::string currentSoundName;
	bool soundIsPlaying;
	bool soundIsLooping;
	float soundPlayer_volume;
	float soundPlayer_speed;
	float soundPlayer_pan;
	bool hearYourself;

	// Settings
	int getBufferSizeIdx(int bSize);
	int bufferSize_idx;
	int getSampleRateIdx(int sRate);
	int getSampleRate(int idx);
	int sampleRate_idx;
	void changeVideoFolder();

	// Afx
	std::vector<bool> afxs;
	void applyAfxs();
	bool activePitchshift;
	bool activeEq;
	bool activeDelay;
	bool activeDistort;
	bool activeRobot;
	bool activePlaySound;
	bool afx_playSound_isPlaying;
	bool afx_playSound_looping;
	std::vector<float> delayBuffer;
	int delayWritePoint;
	int delayReadPoint;
	float afx_delayTime;
	bool afx_delayOnce;
	bool afx_delayOnly;
	int afx_delayRecursionType;
	float afx_delayVolume;


	// JSON Speicherfunktion
	ofxJSONElement settings;
	void createDefaultSettings();

	// screenshot-Funktion (Zu Dokumentationszwecken)
	ofImage imgScreenshot;
	bool snapped;

	// virtuelle Kamera
	scCamera vCamera;
	ofPixels virtualPixel;
};
