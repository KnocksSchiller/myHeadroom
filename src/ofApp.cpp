#include "ofApp.h"
//ToDo: JSONkram in Setup einrichten und überall sonst die Werte als lokale Attribute nutzen (vlt. besser so?)

//--------------------------------------------------------------
void ofApp::setup() {
	// JSON Einstellungen.
	while (!settings.open("settings.json")) {
		std::cout << "no settings file. creating new .json file with default settings" << endl;
		createDefaultSettings();
	}

	// Setze Fenstergroesse
	ofSetWindowShape(settings["window_w"].asInt(), settings["window_h"].asInt());

	// Initialisierung SoundStream
	initSoundStream();

	// Initialisierung Kamera
	initCamera();

	// Initialisierung Recorder
	recordFbo.allocate(camWidth, camHeight, GL_RGB);
	recorder.setup(true, true, glm::vec2(camWidth, camHeight));
	recorder.setAudioConfig(settings["bufferSize"].asInt(), settings["sampleRate"].asInt());
	recorder.setOverWrite(true);
	recorder.setFFmpegPath(ofToDataPath("ffmpeg"));
	recorder.setFps(vfx_maxFrames);
	recorder.setDefaultAudioDevice(audioDevices[audioInId]);
	recorder.setDefaultVideoDevice(videoDevices[settings["cam_device"].asInt()]);

	// virtuelle Kamera
	vCamera = scCreateCamera(camWidth, camHeight, vfx_maxFrames);
	if (vCamera) std::cout << "Virtual Camera initialited." << std::endl;

	// GUI: 
	gui.setup(nullptr, false, ImGuiConfigFlags_ViewportsEnable, true);
	showSettings = false;

	// Record
	isRecordingAudio = false;
	isRecordingVideo = false;

	// Video
	vfxs.resize(mhVfx::VFXSIZE);
	videoIsPlaying = false;
	videoIsLooping = false;
	videoLoopsBAF = false;
	videoVolume = 1.f;
	videoSpeed = 1.f;
	videoPixel.allocate(camWidth, camHeight, GL_RGBA);
	outTexture.allocate(camWidth, camHeight, GL_RGBA);

	// Vfx
	vfx_recolor_value = settings["vfx_recolor_value"].asInt();
	vfx_recolor_brightness = settings["vfx_recolor_brightness"].asInt();
	vfx_recolor_saturation = settings["vfx_recolor_saturation"].asInt();
	vfx_blur_intensity = settings["vfx_blur_intensity"].asInt();
	vfx_ascii_fontsize = settings["vfx_ascii_fontsize"].asInt();
	vfx_canny_th1 = settings["vfx_canny_th1"].asInt();
	vfx_canny_th2 = settings["vfx_canny_th2"].asInt();

	// stutter
	vfx_stutterBuffer.resize((int)ofGetFrameRate() * settings["stutter_longest"].asFloat());
	afx_stutterBuffer.resize(settings["stutter_longest"].asFloat() * settings["sampleRate"].asInt());
	stutterTime = settings["stutter_time"].asFloat();
	vfx_stutter_writePoint, afx_stutter_writePoint = 0;
	// notwendig?
	afx_stutter_readPoint = getReadPoint(afx_stutter_writePoint,
		stutterTime, settings["sampleRate"].asInt(), afx_stutterBuffer.size());

	// Afx
	afxs.resize(mhAfx::AFXSIZE);
	currentSound = settings["current_sound"].asString();
	currentSoundName = settings["current_sound_name"].asString();
	soundIsLooping = false;
	soundPlayer_volume = 1.f;
	soundPlayer_speed = 1.f;
	soundPlayer_pan = 0.f;
	afx_delayTime = settings["delay_time"].asFloat();
	afx_delayVolume = 1.f;
	delayBuffer.resize(settings["delay_longest"].asFloat() * settings["sampleRate"].asInt());
	delayWritePoint = 0;
	// notwendig?
	delayReadPoint = getReadPoint(delayWritePoint, afx_delayTime,
		settings["sampleRate"].asInt(), delayBuffer.size());

	// Settings
	bufferSize_idx = getBufferSizeIdx(settings["bufferSize"].asInt());
	sampleRate_idx = getSampleRateIdx(settings["sampleRate"].asInt());
}

//--------------------------------------------------------------
void ofApp::update() {
	// Video abspielen
	if (videoIsPlaying) {
		videoPlayer.update();
		if (videoPlayer.isFrameNew()) {
			// Videoinhalt -> Pixel
			videoPixel = videoPlayer.getPixels();
			videoPlayer.setSpeed(videoSpeed);
			videoPlayer.setVolume(videoVolume);
		}
		if (videoPlayer.getIsMovieDone() && !videoIsLooping) videoIsPlaying = false;
		(videoIsLooping ? videoPlayer.setLoopState(OF_LOOP_NORMAL) : videoPlayer.setLoopState(OF_LOOP_NONE));
		// ToDo: rausfinden, warum parlindrome nicht klappt
		if (videoIsLooping && videoLoopsBAF) videoPlayer.setLoopState(OF_LOOP_PALINDROME);

	}
	// Kamera anzeigen
	else {
		camera.update();
		// Kamerainhalt -> Pixel
		if (camera.isFrameNew()) videoPixel = camera.getPixels();
	}

	// Uebernahme Videoeffekte
	if (!activeStutter) applyVfxs();

	// Stottereffekt
	if (stutter_enabled) {
		// Buffer abspielen
		if (activeStutter) {
			videoPixel = vfx_stutterBuffer[vfx_stutter_readPoint];
			vfx_stutter_readPoint = (vfx_stutter_readPoint + 1) % vfx_stutterBuffer.size();
			if (vfx_stutter_readPoint == vfx_stutter_writePoint) vfx_stutter_readPoint = vfx_stutter_readPointStart;
		}
		// Buffer mit Pixeln fuellen
		else {
			vfx_stutterBuffer[vfx_stutter_writePoint] = videoPixel;
			vfx_stutter_writePoint = (vfx_stutter_writePoint + 1) % vfx_stutterBuffer.size();
		}
	}
	// Aktualisiere neue Bildausgabe
	outTexture.loadData(videoPixel);

	if (scIsConnected(vCamera)) {
		// RGB -> BGR wegen Farbschema v. DirectShow API 
		// ofPixels.swapRgb() stuerzt App ab, sobald Inhalt Schwarz-Weiss ist, daher mit ofxCv
		ofxCv::convertColor(videoPixel, virtualPixel, CV_RGB2BGR);
		scSendFrame(vCamera, virtualPixel.getData());
	}

	// Soundplayer-Eigenschaften
	soundPlayer.setLoop(soundIsLooping);
	if (soundPlayer.getVolume() != soundPlayer_volume) soundPlayer.setVolume(soundPlayer_volume);
	if (soundPlayer.getSpeed() != soundPlayer_speed) soundPlayer.setSpeed(soundPlayer_speed);
	if (soundPlayer.getPan() != soundPlayer_pan) soundPlayer.setPan(soundPlayer_pan);
}

//--------------------------------------------------------------
void ofApp::draw() {
	// Zeichnen GUI
	gui.begin();

	// ToDo: Fonts einbauen
	// Menueleiste, ToDo: Hoehe anpassen
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Optionen")) {
			if (ImGui::Button("Einstellungen")) showSettings = true;
			// Todo: Aspect-Ratio Auswahl

			ImGui::EndMenu();
		}
		ImGui::Text("FPS: ");
		ImGui::SameLine();
		ImGui::Text(std::to_string(ofGetFrameRate()).c_str());
	}
	ImGui::EndMainMenuBar();

	// Galerie Fenster
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos + ImVec2((ofGetWidth() * .6f), 0), ImGuiCond_Once);
	ImGui::Begin("Galerie", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	// Inhalt Galerie Fenster
	listVideos(ImGui::GetWindowWidth());

	ImGui::End();

	// Presets Fenster
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos +
		ImVec2((ofGetWidth() * .6f), 0 + (ofGetHeight() / 3)), ImGuiCond_Once);
	//ImGui::SetNextWindowSize(ImVec2(ofGetWidth() * .4f, ofGetHeight() / 3), ImGuiCond_Once);
	ImGui::Begin("Presets", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	// Inhalt Preset Fenster (Todo)

	ImGui::End();

	// Effekte-Tab
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos +
		ImVec2((ofGetWidth() * .6f), 0 + (2 * (ofGetHeight() / 3))), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(ofGetWidth() * .4f, ofGetHeight() / 3), ImGuiCond_Once);
	ImGui::Begin("A/V-Effekte", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
	if (ImGui::BeginTabBar("Effekte")) {
		if (ImGui::BeginTabItem("Video")) {
			// Videoeffekte auflisten
			listVfxs();

			// Annuliert vfx Zustaende
			if (ImGui::Button("Leeren")) {
				activeRecolor = false;
				activeCanny = false;
				activeChroma = false;
				activeMosaicCircle = false;
				activeBNW = false;
				activeBlur = false;
				activeFrameRate = false;
				activeNegative = false;
				activeMirror = false;
				activeAscii = false;
				// Aus irgendeinem Grund setzt openFrameworks videoIsPlaying auf true, 
				// wenn ich genau diesen Button druecke (es gibt keine Zeile, dass es das tun sollte). 
				// Daher nochmal hier auf false setzen
				videoIsPlaying = false;

				for (int i = 0; i < vfxs.size(); i++) vfxs[i] = false;
			}
			ImGui::EndTabItem();
		}
		// Auswahl Videoeingabe
		if (ImGui::TabItemButton("Kameras")) {
			ImGui::OpenPopup("Available Cams");
		}
		if (ImGui::BeginPopup("Available Cams")) {
			// Kameras auflisten und auswaehlen
			for (int i = 0; i < videoDevices.size(); i++) {
				if (videoDevices[i].id == settings["cam_device"].asInt())
					ImGui::Selectable(videoDevices[i].deviceName.c_str(), true);
				else if (videoDevices[i].deviceName != "DirectShow Softcam" && ImGui::Selectable(videoDevices[i].deviceName.c_str(), false)) {
					camera.close();
					settings["cam_device"] = videoDevices[i].id;
					camera.setDeviceID(videoDevices[i].id);
					camera.initGrabber(camWidth, camHeight);
					recorder.setDefaultVideoDevice(videoDevices[settings["cam_device"].asInt()]);
					std::cout << "Changed camera to: " << videoDevices[i].deviceName << std::endl;
				}
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginTabItem("Audio")) {
			listAfxs();

			// Annuliert Afx-Zustaende
			if (ImGui::Button("Leeren")) {
				activePitchshift = false;
				activeEq = false;
				activeDelay = false;
				activeDistort = false;
				activeRobot = false;
				activePlaySound = false;
				for (int i = 0; i < afxs.size(); i++) afxs[i] = false;
			}
			ImGui::EndTabItem();
		}
		// Auswahl Toneingabe
		if (ImGui::TabItemButton("Mics")) {
			ImGui::OpenPopup("Available Mics");
		}
		if (ImGui::BeginPopup("Available Mics")) {
			// Mics auflisten und auswaehlen
			for (int i = 0; i < audioDevices.size(); i++) {
				if (audioDevices[i].deviceID == settings["audioInId"].asInt())
					ImGui::Selectable(audioDevices[i].name.c_str(), true);
				else if (audioDevices[i].inputChannels > 0)
					if (ImGui::Selectable(audioDevices[i].name.c_str(), false)) {
						soundStream.stop();
						soundStream.close();
						audioInId = audioDevices[i].deviceID;
						settings["audioInId"] = audioDevices[i].deviceID;
						soundSettings.setInDevice(audioDevices[i]);
						soundSettings.numInputChannels = audioDevices[i].inputChannels;
						soundStream.setup(soundSettings);
						recorder.setDefaultAudioDevice(audioDevices[audioInId]);
						inputBuffer.resize(soundStream.getBufferSize() * soundStream.getNumInputChannels());
						std::cout << "Changed input to: " << audioDevices[i].name << endl;
					}
			}
			ImGui::EndPopup();
		}
		if (ImGui::TabItemButton("Output")) {
			ImGui::OpenPopup("outputs");
		}
		if (ImGui::BeginPopup("outputs")) {
			for (int i = 0; i < audioDevices.size(); i++) {
				if (audioDevices[i].deviceID == settings["audioOutId"].asInt())
					ImGui::Selectable(audioDevices[i].name.c_str(), true);
				else if (audioDevices[i].outputChannels > 0)
					if (ImGui::Selectable(audioDevices[i].name.c_str(), false)) {
						soundStream.stop();
						soundStream.close();
						audioOutId = audioDevices[i].deviceID;
						settings["audioOutId"] = audioDevices[i].deviceID;
						soundSettings.setOutDevice(audioDevices[i]);
						soundSettings.numOutputChannels = audioDevices[i].outputChannels;
						soundStream.setup(soundSettings);
						std::cout << "Changed output to: " << audioDevices[i].name << endl;
					}
			}
			ImGui::EndPopup();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();

	// Linke Haelfte Fenster 1: Einstellungen, Aufnehmen
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos +
		ImVec2((ofGetWidth() * .05f), 0 + (2 * ofGetHeight() / 3) - 25), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(ofGetWidth() * .15f, ofGetHeight() / 3), ImGuiCond_Once);
	ImGui::Begin("L1", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
	if (ImGui::Button("Einstellungen", ImVec2(0, 30))) showSettings = true;
	ImGui::SameLine();
	if (ImGui::Button("Aufnehmen", ImVec2(0, 60))) {
		// Aufnahme beenden
		if (recorder.isRecording()) {
			recorder.stop();
			isRecordingVideo = false;
			// Lade aufgenommenes Video videoPath
			videoPlayer.load(settings["current_video"].asString());
		}
		else {
			// Aufnahme starten
			currentVideo = settings["video_path"].asString() +
				settings["video_fileName"].asString() +
				ofGetTimestampString() +
				settings["video_format"].asString();
			recorder.setOutputPath(currentVideo);
			// speichere letztes video in settings 
			// ToDo: entweder das weg oder erst nachdem beenden umschreiben
			settings["current_video"] = currentVideo;
			//recorder.setVideoCodec("libx264");
			//recorder.setAudioCodec("aac");
			recorder.setBitRate(settings["video_bitrate"].asInt());
			isRecordingVideo = true;
			recorder.startCustomRecord();
			//recorder.startMhRecord();
		}

	}
	// Aufnahmezeit: isRecordingVideo, isRecordingAudio
	ImGui::Text(std::to_string(recorder.getRecordedDuration()).c_str());
	//ImGui::Text(std::to_string(recorder.getRecordedAudioDuration(audioFPS)).c_str());

	// Laesst dich selbst hoeren wenn noetig
	ImGui::Checkbox("Hoere dich selbst", &hearYourself);

	// Einstellungsfenster
	if (showSettings) {
		ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Once);
		ImGui::Begin("Einstellungen", &showSettings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		if (ImGui::Combo("Buffer Size", &bufferSize_idx, " 64\0 128\0 256\0 512\0 1024\0 2048\0")) {
			int currentBSize = 64;
			for (int i = 0; i < bufferSize_idx; i++) {
				currentBSize *= 2;
			}
			settings["bufferSize"] = currentBSize;
			soundStream.stop();
			soundStream.close();
			soundSettings.bufferSize = currentBSize;
			soundStream.setup(soundSettings);
			recorder.setAudioConfig(currentBSize, settings["sampleRate"].asInt());
			inputBuffer.resize(currentBSize * soundStream.getNumInputChannels());
			settings.save("settings.json", true);
			std::cout << "Changed Buffer Size to: " << currentBSize << std::endl;
		}
		// Bug: Absturz, wenn man SampleRate von entweder 44100/11025 auf andere Rate wechselt (HEAP_CORRUPTION).
		if (ImGui::Combo("Sample rate", &sampleRate_idx, " 8000\0 11025 (don't)\0 16000\0 22050\0 44100 (don't)\0 48000\0 88200\0 96000\0")) {
			int sRate = getSampleRate(sampleRate_idx);
			settings["sampleRate"] = sRate;
			soundStream.stop();
			soundStream.close();
			soundSettings.sampleRate = sRate;
			soundStream.setup(soundSettings);
			afx_stutterBuffer.resize(settings["stutter_longest"].asFloat() * sRate);
			afx_stutter_readPoint = getReadPoint(afx_stutter_writePoint,
				stutterTime, sRate, afx_stutterBuffer.size());
			recorder.setAudioConfig(settings["bufferSize"].asInt(), sRate);
			delayBuffer.resize(settings["delay_longest"].asFloat() * sRate);
			settings.save("settings.json", true);
			std::cout << "Changed Samplerate to: " << sRate << std::endl;
		}
		ImGui::BeginGroup();
		ImGui::Text("Videopfad: ");
		if (ImGui::Button("Aendern")) {
			changeVideoFolder();
		}
		ImGui::EndGroup();
		ImGui::SameLine();
		ImGui::TextWrapped(settings["video_path"].asString().c_str());


		ImGui::End();
	}
	ImGui::End();

	// Linke Haelfte 2: Stotterzeit, Stottermodus, Videooptionen, Abspielen, Loop-BAF, Loop
	ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos +
		ImVec2((ofGetWidth() * .2f + 25), 0 + (2 * ofGetHeight() / 3) - 25), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(ofGetWidth() * .35f, ofGetHeight() / 3), ImGuiCond_Once);
	ImGui::Begin("L2", 0, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
	ImGui::SetNextItemWidth(ImGui::GetWindowWidth() / 3);
	if (ImGui::InputFloat("Stotterzeit", &stutterTime, 1.f / (int)ofGetFrameRate(), .2f)) {
		if (stutterTime < (1.f / (int)ofGetFrameRate())) stutterTime = 1.f / (int)ofGetFrameRate();
		else if (stutterTime > (settings["stutter_longest"].asFloat()))
			stutterTime = settings["stutter_longest"].asFloat();
		settings["stutter_time"] = stutterTime;
		// Todo: readPoint und readPointStart aktualisieren, sodass beim Stottern beliebig verlaengert / verkuerzt werden kann
	}
	ImGui::SetItemTooltip("in Sekunden. Schritte abhaengig von App-FPS");
	ImGui::SameLine();
	if (ImGui::Button("Stottern", ImVec2(0, 30))) stutter_enabled = !stutter_enabled;
	ImGui::SetNextItemWidth(ImGui::GetWindowWidth() / 3);
	ImGui::BeginGroup();
	ImGui::SliderFloat("Lautstarke", &videoVolume, 0.f, 1.f, "%.2f");
	ImGui::SetNextItemWidth(ImGui::GetWindowWidth() / 3);
	ImGui::SliderFloat("Geschwindigkeit", &videoSpeed, .25f, 5.f, "%.2f", ImGuiSliderFlags_Logarithmic);
	if (ImGui::Button("Zuruecksetzen", ImVec2(0, 20))) {
		videoVolume = 1.f;
		videoSpeed = 1.f;
	}
	ImGui::EndGroup();
	ImGui::SameLine();
	if (ImGui::Button("Abspielen", ImVec2(0, 60))) {
		if (stutter_enabled) startStopStutter();
		else startStopVideoPlayback();

	}
	ImGui::Checkbox("Vor und zurueck?", &videoLoopsBAF);
	ImGui::SameLine();
	if (ImGui::Button("Loop", ImVec2(0, 30))) videoIsLooping = !videoIsLooping;
	ImGui::SetItemTooltip("Loop");

	ImGui::End();

	gui.end();


	// Kreis und Rand um Kamera beim Aufnehmen
	ofPushStyle(); {
		if (isRecordingVideo) ofSetColor(255, 0, 0);
		else if (stutter_enabled) ofSetColor(255, 255, 0);
		else ofSetColor(0, 220, 0);
		ofDrawCircle(camPosX + camWidth + 20, camPosY + 10, 10);

		//if (isRecordingVideo) ofDrawRectangle(camPosX - 5, camPosY - 5, camWidth + 10, camHeight + 10);
	}
	ofPopStyle();

	// Aufnahmefenster, je nach Kontext entweder Videodatei oder Kamera
	recordFbo.begin(); {
		outTexture.draw(0, 0);
	} recordFbo.end();

	// Aufnahmeprozess, lese Inhalt Fbo in ofPixel rein
	if (recorder.isRecording() && isRecordingVideo) {
		//recordFbo.readToPixels(recordPixel);
		fastFbo.readToPixels(recordFbo, recordPixel);
		if (recordPixel.getWidth() > 0 && recordPixel.getHeight() > 0)
			recorder.addFrame(recordPixel);
	}

	recordFbo.draw(camPosX, camPosY);
	waveform.draw();
	gui.draw();
}

int ofApp::getReadPoint(int writePoint, float time, int factor, int size) {
	return ((writePoint - (int)(time * factor)) + size) % size;
}

void ofApp::startStopStutter() {
	if (activeStutter) {
		vfx_stutter_writePoint, vfx_stutter_readPoint, afx_stutter_readPoint, afx_stutter_writePoint = 0;
		activeStutter = false;
	}
	else {
		vfx_stutter_readPoint = getReadPoint(vfx_stutter_writePoint, stutterTime, (int)ofGetFrameRate(), vfx_stutterBuffer.size());
		vfx_stutter_readPointStart = vfx_stutter_readPoint;
		afx_stutter_readPoint = getReadPoint(afx_stutter_writePoint,
			stutterTime, settings["sampleRate"].asInt(), afx_stutterBuffer.size());
		afx_stutter_readPointStart = afx_stutter_readPoint;
		activeStutter = true;
		//cout << "vfx_readpoint: " << vfx_stutter_readPoint << endl;
		//cout << "afx_readpoint: " << afx_stutter_readPoint << endl;
	}
}

// Todo: Auswahl letztes Video aus Video-Ordner, load-play in try catch und Warnung, dass Abspielen nicht moeglich wurde
// Todo: Abspielen während der Aufnahme fixen. Gibt Warnung NOT_SUPPORTED: speed 1.06x o.ä.
void ofApp::startStopVideoPlayback() {
	// Beende Abspielen Video
	if (videoIsPlaying) {
		videoPlayer.stop();
		videoIsPlaying = false;
	}
	// Ausgewaehltes Video abspielen, letztes aufgenommenes Video als default
	else {
		if (currentVideo.empty()) currentVideo = settings["current_video"].asString();	// bringt theorethisch nichts, wenn kein Video bisher deklariert wurde (z.B. aus default-settings)
		videoPlayer.load(settings["current_video"].asString());
		videoPlayer.play();
		videoIsPlaying = true;
	}
}

void ofApp::listVfxs() {
	if (ImGui::Checkbox("Recolor", &activeRecolor)) vfxs[mhVfx::Recolor] = activeRecolor;
	ImGui::SameLine();
	if (ImGui::TreeNode("Recolor", "")) {
		// Farbton
		ImGui::SliderInt("Farbton", &vfx_recolor_value, -180, 180);
		// Helligkeit - Slider
		ImGui::SliderInt("Helligkeit", &vfx_recolor_brightness, -100, 100);
		// Saettigung Slider
		ImGui::SliderInt("Saettigung", &vfx_recolor_saturation, -100, 100);
		// Alphakanal
		ImGui::SliderInt("Alpha", &vfx_recolor_alpha, 0, 255);
		// camera.videoSettings() uebernimmt quasi all das

		if (ImGui::Button("Zuruecksetzen")) {
			vfx_recolor_value = 0;
			vfx_recolor_brightness = 0;
			vfx_recolor_saturation = 0;
		}

		ImGui::TreePop();
	}

	if (ImGui::Checkbox("Schwarz-Weiss", &activeBNW)) vfxs[mhVfx::Schwarz_Weiss] = activeBNW;

	if (ImGui::Checkbox("Chroma-Key", &activeChroma)) vfxs[mhVfx::Chroma_Key] = activeChroma;
	ImGui::SameLine();
	if (ImGui::TreeNode("Chroma-Key", "")) {
		ImGui::Text("'cause I do it too!");
		// Auswahl zu entfernende Farbe

		// Auswahl Hintergrundbild, -video oder oF-Animation (tbd)

		ImGui::TreePop();
	}

	if (ImGui::Checkbox("Spiegeln", &activeMirror)) vfxs[mhVfx::Mirror] = activeMirror;
	ImGui::SameLine();
	if (ImGui::TreeNode("Spiegeln", "")) {
		ImGui::Checkbox("vertikal", &vfx_mirror_v);
		ImGui::SameLine();
		ImGui::Checkbox("horizontal", &vfx_mirror_h);
		ImGui::TreePop();
	}

	if (ImGui::Checkbox("Mosaic-Circles", &activeMosaicCircle)) vfxs[mhVfx::Mosaic_Circles] = activeMosaicCircle;
	ImGui::SameLine();
	if (ImGui::TreeNode("Mosaic-Circles", "")) {
		ImGui::Text("So put your hands up your face!");
		// Kreisgroesse Slider

		ImGui::TreePop();
	}

	if (ImGui::Checkbox("Blur", &activeBlur)) vfxs[mhVfx::Blur] = activeBlur;
	ImGui::SameLine();
	if (ImGui::TreeNode("Blur", "")) {
		ImGui::RadioButton("Blur", &vfx_blur_type, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Gaussian Blur", &vfx_blur_type, 1);
		ImGui::SameLine();
		ImGui::RadioButton("Median Blur", &vfx_blur_type, 2);
		ImGui::SliderInt("Intensitaet", &vfx_blur_intensity, 0, 100);
		if (ImGui::Button("Standard")) vfx_blur_intensity = 50;
		ImGui::TreePop();
	}

	if (ImGui::Checkbox("Negativ", &activeNegative)) vfxs[mhVfx::Negative] = activeNegative;

	if (ImGui::Checkbox("Frame-Rate", &activeFrameRate)) vfxs[mhVfx::FrameRate] = activeFrameRate;
	ImGui::SameLine();
	if (ImGui::TreeNode("Framerate", "")) {
		ImGui::SameLine();
		ImGui::SliderInt("FPS", &vfx_currentFrames, 1, vfx_maxFrames);
		if (ImGui::Button("Zuruecksetzen")) {
			vfx_currentFrames = vfx_maxFrames;
		}
		ImGui::TreePop();
	}

	if (ImGui::Checkbox("Ascii", &activeAscii)) vfxs[mhVfx::Ascii] = activeAscii;
	ImGui::SameLine();
	if (ImGui::TreeNode("Ascii", "")) {
		ImGui::SameLine();
		ImGui::SliderInt("Groesse", &vfx_ascii_fontsize, 1, 32);
		ImGui::TreePop();
	}

	if (ImGui::Checkbox("Canny Edge-Detection", &activeCanny)) vfxs[mhVfx::Canny_Edges] = activeCanny;
	ImGui::SameLine();
	if (ImGui::TreeNode("Canny", "")) {
		// Slider Threshold 1, 2, Dilation, resize
		ImGui::SliderInt("Threshold 1", &vfx_canny_th1, 0, 300);
		ImGui::SliderInt("Threshold 2", &vfx_canny_th2, 1, 600);
		ImGui::SliderInt("Dilation", &vfx_canny_dilation, 0, 10);
		ImGui::SliderInt("Resize", &vfx_canny_resize, 1, 200);
		ImGui::TreePop();
	}

}


void ofApp::listAfxs() {
	if (ImGui::Checkbox("Pitch-Shift", &activePitchshift)) afxs[mhAfx::pitchShift] = activePitchshift;
	ImGui::SameLine();
	if (ImGui::TreeNode("Pitch-Shift", "")) {
		ImGui::Text("We are not lovers");
		// Slider 
		ImGui::TreePop();
	}
	if (ImGui::Checkbox("Equalizer", &activeEq)) afxs[mhAfx::equalizer] = activeEq;
	ImGui::SameLine();
	if (ImGui::TreeNode("Equalizer", "")) {
		ImGui::Text("We are not romantics");
		// Slider 

		ImGui::TreePop();
	}
	if (ImGui::Checkbox("Delay", &activeDelay)) afxs[mhAfx::delay] = activeDelay;
	ImGui::SameLine();
	if (ImGui::TreeNode("Delay", "")) {
		// Input Delayzeit
		if (ImGui::InputFloat("Verzoegerungszeit", &afx_delayTime, .01f, .1f, "%.2f")) {
			if (afx_delayTime > settings["delay_longest"].asFloat()) afx_delayTime = settings["delay_longest"].asFloat();
			else if (afx_delayTime <= 0) afx_delayTime = .01f;
			delayReadPoint = getReadPoint(delayWritePoint, afx_delayTime,
				settings["sampleRate"].asInt(), delayBuffer.size());
			settings["delay_time"] = afx_delayTime;
		}
		// Radio einmal oder Recursion
		ImGui::Text("Nachhall-Typ:");
		ImGui::RadioButton("Einmalig", &afx_delayRecursionType, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Nachhall", &afx_delayRecursionType, 1);
		// Checkbox Delay only?
		ImGui::Checkbox("Delay only", &afx_delayOnly);
		// Slider Nachklang Lautstaerke
		ImGui::SliderFloat("Lautstaerke", &afx_delayVolume, 0.f, 1.f, "%.2f");

		ImGui::TreePop();
	}
	if (ImGui::Checkbox("Distort", &activeDistort)) afxs[mhAfx::distort] = activeDistort;
	ImGui::SameLine();
	if (ImGui::TreeNode("Distort", "")) {
		ImGui::Text("We are here to serve you");
		// Slider 

		ImGui::TreePop();
	}
	if (ImGui::Checkbox("Robot", &activeRobot)) afxs[mhAfx::robot] = activeRobot;
	ImGui::SameLine();
	if (ImGui::TreeNode("Robot", "")) {
		ImGui::Text("A different face,");
		// Slider Hoehe Sinuswelle (oder so) 

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Play Sound")) {
		// Auswahl Datei
		if (ImGui::Button("Sound auswahlen")) {
			ofFileDialogResult result = ofSystemLoadDialog("Lade Sound", false,
				ofToDataPath(settings["sound_path"].asString(), true));
			if (result.bSuccess) {
				settings["current_sound_name"], currentSoundName = result.getName();
				soundPlayer.load(result.getPath());
				settings["current_sound"] = result.getPath();
			}
		}
		ImGui::SameLine();
		ImGui::Text(currentSoundName.c_str());
		// Abspielbutton
		if (ImGui::Button("Abspielen")) {
			if (soundPlayer.isPlaying()) {
				soundPlayer.stop();
			}
			else {
				if (currentSound.empty()) currentSound = settings["current_sound"].asString();
				soundPlayer.load(settings["current_sound"].asString());
				soundPlayer.play();
			}
		}
		// Loop-Button
		ImGui::SameLine();
		ImGui::Checkbox("Loop", &soundIsLooping);

		// Lautstaerke, Geschwindigkeit, Panorama (L-/R-Verhaeltnis)
		ImGui::SliderFloat("Lautstaerke", &soundPlayer_volume, 0.f, 2.f, "%.2f");
		ImGui::SliderFloat("Geschwindigkeit", &soundPlayer_speed, .25f, 5.f, "%.2f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Panorama", &soundPlayer_pan, -1.f, 1.f, "%.2f");
		if (ImGui::Button("Zuruecksetzen")) {
			soundPlayer_volume = 1.f;
			soundPlayer_speed = 1.f;
			soundPlayer_pan = 0.f;
		}
		ImGui::TreePop();
	}
}

void ofApp::changeVideoFolder() {
	ofFileDialogResult result = ofSystemLoadDialog("Videopfad", true,
		settings["video_path"].asString());
	if (result.bSuccess && std::filesystem::is_directory(result.getPath())) {
		settings["video_path"] = result.getPath() + "\\";
		settings.save("settings.json", true);
	}
}

std::string ofApp::shortenFileName(const std::string& fileName, size_t limit) {
	if (fileName.length() <= limit) return fileName;
	if (limit <= 3) return "...";
	return "..." + fileName.substr(fileName.length() - (limit - 3));
}

void ofApp::listVideos(int wWidth) {
	float gridWidth = wWidth - 60;
	std::string vidPath = settings["video_path"].asString();
	if (std::filesystem::exists(vidPath) && std::filesystem::is_directory(vidPath)) {
		int n = 0;
		for (const auto& entry : std::filesystem::directory_iterator(vidPath)) {
			if (entry.path().string().find(".mp4") != std::string::npos) {
				if (n % 3) ImGui::SameLine();
				if (entry.path() == settings["current_video"].asString())
					ImGui::Selectable(shortenFileName(entry.path().filename().string(), 22).c_str(), true,
						ImGuiSelectableFlags_None, ImVec2(gridWidth / 3.f, 0));
				else if (ImGui::Selectable(shortenFileName(entry.path().filename().string(), 22).c_str(), false,
					ImGuiSelectableFlags_None, ImVec2(gridWidth / 3.f, 0))) {
					settings["current_video"] = entry.path().string();
					videoPlayer.load(entry.path().string().c_str());
				}
				n++;
			}
		}
		if (n == 0) {
			ImGui::TextWrapped("Keine Videos im ausgewaehlten Ordner.");
			if (ImGui::Button("Videopfad aendern")) changeVideoFolder();
		}
	}
}

void ofApp::applyVfxs() {
	(videoIsPlaying ? ofxCv::copy(videoPlayer, videoPixel) : ofxCv::copy(camera, videoPixel));
	for (int i = 0; i < vfxs.size(); i++) {
		if (vfxs[i]) {
			switch (i) {
			case mhVfx::Recolor:
				break;
			case mhVfx::Schwarz_Weiss:
				ofxCv::copyGray(videoPixel, tempPixel);
				videoPixel = tempPixel;
				break;
			case mhVfx::Chroma_Key:
				break;
			case mhVfx::Mosaic_Circles:
				break;
			case mhVfx::Blur:
				switch (vfx_blur_type) {
				case 0:	ofxCv::blur(videoPixel, vfx_blur_intensity); break;
				case 1:	ofxCv::GaussianBlur(videoPixel, vfx_blur_intensity); break;
				case 2: ofxCv::medianBlur(videoPixel, vfx_blur_intensity); break;
				default: std::cout << "blur type not found" << endl;
				} break;
			case mhVfx::Negative:
				for (size_t i = 0; i < videoPixel.size(); i++) videoPixel[i] = 255 - videoPixel[i];
				break;
			case mhVfx::FrameRate:
				break;
			case mhVfx::Mirror: videoPixel.mirror(vfx_mirror_v, vfx_mirror_h); break;
			case mhVfx::Canny_Edges:
				ofxCv::copyGray(videoPixel, tempPixel);
				// resize, minimierte so multiplizieren, sodass es wieder 1 wird
				ofxCv::resize(tempPixel, videoPixel, 
					((videoPixel.getWidth() / videoPixel.getHeight()) * ((double) vfx_canny_resize / 100)), 
					(1 * ((double) vfx_canny_resize / 100)));
				ofxCv::Canny(videoPixel, tempPixel, vfx_canny_th1, vfx_canny_th2);
				ofxCv::dilate(tempPixel, videoPixel, vfx_canny_dilation);
				break;
				//default:
			}
		}
	}
}

void ofApp::applyAfxs() {
	// gehe durch Audioeffekte durch
	for (int i = 0; i < afxs.size(); i++) {
		if (afxs[i]) {
			switch (i) {
			case mhAfx::pitchShift:
				break;
			case mhAfx::equalizer:
				break;
			case mhAfx::delay:
				delaySample = delayBuffer[delayReadPoint];
				// Rekursionstyp einmalig
				if (afx_delayRecursionType == 0) delayBuffer[delayWritePoint] = outSampleL;
				// Rekursionstyp Nachhall
				else delayBuffer[delayWritePoint] = outSampleL + (delaySample * afx_delayVolume);

				delayWritePoint = (delayWritePoint + 1) % delayBuffer.size();
				delayReadPoint = (delayReadPoint + 1) % delayBuffer.size();

				//nur delay ? cS = dS : cS += dS 
				if (afx_delayOnly) {
					outSampleL = delaySample * afx_delayVolume;
					outSampleR = delaySample * afx_delayVolume;
				}
				else {
					outSampleL += delaySample * afx_delayVolume;
					outSampleR += delaySample * afx_delayVolume;
				}
				break;
			case mhAfx::robot:
				break;
				//default:
			}
		}
	}
}

void ofApp::audioIn(ofSoundBuffer& input) {
	if (ofGetElapsedTimeMillis() - lastAudioTimeReset >= 1000) {
		lastAudioTimeReset = ofGetElapsedTimeMillis();
		audioFPS = audioCounter;
		audioCounter = 0;
	}
	else {
		audioCounter++;
	}

	//if (recorder.isRecording() && isRecordingAudio) {
	//	recorder.addBuffer(input, audioFPS);
	//}

	waveform.clear();
	for (size_t i = 0; i < input.getNumFrames(); i++) {
		float sample = input.getSample(i, 0);
		float x = ofMap(i, 0, input.getNumFrames(), 0, ofGetWidth() * 0.6f);
		float y = ofMap(sample, -1, 1, 0, ofGetHeight());
		waveform.addVertex(x, y + 50);

		// Toneingang in neuen Buffer, um es ausserhalb nutzen zu koennen
		inputBuffer[i * input.getNumChannels() + 0] = sample;
		if (input.getNumChannels() == 2) inputBuffer[i * input.getNumChannels() + 1] = input.getSample(i, 1);
	}

	//bufferCounter++;

}

void ofApp::audioOut(ofSoundBuffer& buffer) {
	if (hearYourself) {
		for (int i = 0; i < buffer.getNumFrames(); i++) {
			outSampleL = inputBuffer[i * soundStream.getNumInputChannels() + 0];
			(soundStream.getNumInputChannels() == 2 ? outSampleR = inputBuffer[i * soundStream.getNumInputChannels() + 1]
				: outSampleR = outSampleL);

			applyAfxs();

			// Stottern, Audio
			if (stutter_enabled) {
				if (!activeStutter) {
					afx_stutterBuffer[afx_stutter_writePoint] = outSampleL;
					afx_stutter_writePoint = (afx_stutter_writePoint + 1) % afx_stutterBuffer.size();
				}
				else {
					outSampleL = afx_stutterBuffer[afx_stutter_readPoint];
					outSampleR = afx_stutterBuffer[afx_stutter_readPoint];
					afx_stutter_readPoint = (afx_stutter_readPoint + 1) % afx_stutterBuffer.size();
					if (afx_stutter_readPoint == afx_stutter_writePoint) afx_stutter_readPoint = afx_stutter_readPointStart;
				}
			}

			buffer[i * buffer.getNumChannels() + 0] = outSampleL;
			buffer[i * buffer.getNumChannels() + 1] = outSampleR;
		}

		// ToDo: Waveform fuer Ausgang einfuegen, falls moeglich

	}

}

// Prueft, ob IDs valide sind. Wechselt bei Ungueltigkeit zu default Input / Output.
// finde keine Loesung den Code nicht duplizieren zu muessen ;(
void ofApp::checkIOId(int id, bool isInput) {
	if (isInput) {
		if (id >= audioDevices.size() || audioDevices[id].inputChannels <= 0) {
			for (int i = 0; i < audioDevices.size(); i++) {
				if (audioDevices[i].inputChannels > 0 && audioDevices[i].isDefaultInput) {
					audioInId = i;
					std::cout << "input-device configured with id: " << i << endl;
					break;
				}
			}
		}
	}
	else {
		if (id >= audioDevices.size() || audioDevices[id].outputChannels <= 0) {
			for (int i = 0; i < audioDevices.size(); i++) {
				if (audioDevices[i].outputChannels > 0 && audioDevices[i].isDefaultOutput) {
					audioOutId = i;
					std::cout << "output-device configured with id: " << i << endl;
					break;
				}
			}
		}
	}
}

void ofApp::initSoundStream() {
	audioDevices = ofSoundStreamListDevices();
	audioInId = settings["audioInId"].asInt();
	audioOutId = settings["audioOutId"].asInt();
	// Preufen, ob IDs valide sind
	checkIOId(audioInId);
	checkIOId(audioOutId, false);
	// Einstellungen Audio I/O, usw.
	soundSettings.setOutDevice(audioDevices[audioOutId]);
	soundSettings.setInDevice(audioDevices[audioInId]);
	soundSettings.setInListener(this);
	soundSettings.setOutListener(this);
	soundSettings.sampleRate = settings["sampleRate"].asInt();
	soundSettings.numInputChannels = audioDevices[audioInId].inputChannels;
	soundSettings.numOutputChannels = audioDevices[audioOutId].outputChannels;
	soundSettings.bufferSize = settings["bufferSize"].asInt();
	soundSettings.numBuffers = settings["num_outChannels"].asInt() * 2;
	soundStream.setup(soundSettings);

	settings["audioInId"] = audioInId;
	settings["audioOutId"] = audioOutId;

	audioFPS = 0.0f;
	audioCounter = 0;
	bufferCounter = 0;
	lastAudioTimeReset = ofGetElapsedTimeMillis();
	hearYourself = false;
	inputBuffer.resize(soundStream.getBufferSize() * soundStream.getNumInputChannels());
}

/*
Bug! Bsp. Hoehe 798 mach camWidth 399, setup macht Seitenverhaeltnis auf 16:9 auch wenn
Kamera-Seitenverhältnis auf 4:3 gesetzt wird. Evtl. Seitenverhaeltnisse besser konfigurieren
mit eigener Methode? Falls Ratio 1.33 dann 640x480?.
Passiert bei zu kleiner Fenstergroesse, wenn camHeight naeher an 360 als an 480 ist

Bug! Bei Fensterbreite 800x798 ist Kamera Pos x < 0, da Breite von der Hoehe abhaengig ist. Evtl. so umschreiben,
dass entweder Breite oder Hoehe der Hauptpunkt sind?
*/
void ofApp::initCamera() {
	videoDevices = camera.listDevices();
	if (settings["cam_device"].asInt() >= videoDevices.size()) settings["cam_device"] = 0;
	// Ermittlung der Kamera ueber OpenCV
	cv::VideoCapture cap(settings["cam_device"].asInt());
	// Setzen der hoechst moeglichen Kameraaufloesung und fps, wird dann in cap.set() automatisch beschraenkt
	cap.set(cv::CAP_PROP_FRAME_WIDTH, 4000);
	cap.set(cv::CAP_PROP_FRAME_HEIGHT, 4000);
	cap.set(cv::CAP_PROP_FPS, 200);

	// Berechnung Seitenverhaeltnis, neue Breiteund X-Position innerhalb App
	//aspectRatio = cap.get(cv::CAP_PROP_FRAME_WIDTH) / cap.get(cv::CAP_PROP_FRAME_HEIGHT);
	//camHeight = (float)ofGetHeight() / 2;
	camHeight = 360;
	//camWidth = camHeight * aspectRatio;
	camWidth = 640;
	camPosX = (settings["window_w"].asFloat() * (3.f / 5.f)) / 2.f - (camWidth / 2.f);
	camPosY = (float)ofGetHeight() * 0.05;

	// Initialisieren Kamera mit neue Groesse
	camera.setDeviceID(settings["cam_device"].asInt());
	camera.setup(camWidth, camHeight);

	// fuer vfx::FrameRate
	vfx_maxFrames = cap.get(cv::CAP_PROP_FPS);
	vfx_currentFrames = vfx_maxFrames;

	// Log
	//std::cout << "Ratio: " << settings["aspectRatio"].asFloat() << "\n Breite: " << camWidth << "\n Hoehe: " << camHeight <<
		//"\n Resised Cam-Heigh: " << resizedCamHeight << "\n Pos-Y: " << camPosY << "\n Pos-X: " <<
		//camPosX << "\n fps: " << cap.get(cv::CAP_PROP_FPS) << std::endl;
}

void ofApp::createDefaultSettings() {
	settings["buffersize"] = 1024;
	settings["cam_device"] = 0;
	settings["hotkey_screenshot"] = 3681;	// r-shift default
	settings["num_inChannels"] = 1;
	settings["num_outChannels"] = 2;
	settings["sampleRate"] = 48000;
	settings["screenshot_fileName"] = "screenshot_";
	settings["screenshot_format"] = ".png";
	settings["screenshot_number"] = 0;
	settings["screenshot_path"] = "screenshots/";
	settings["screenshot_withTime"] = true;
	settings["window_h"] = 798;
	settings["window_w"] = 1366;
	settings["active_vfx"] = -1;
	settings["active_blur"] = false;
	settings["active_bnw"] = false;
	settings["vfx_recolor_value"] = 0;
	settings["vfx_recolor_brightness"] = 0;
	settings["vfx_recolor_saturation"] = 0;
	settings["vfx_blur_intensity"] = 50;
	settings["video_path"] = ofToDataPath("videos/", true);
	settings["video_fileName"] = "video_";
	settings["video_format"] = ".mp4";
	settings["current_video"] = "";
	settings["vfx_ascii_fontsize"] = 12;
	settings["current_sound"] = "";
	settings["sound_path"] = "sounds/";
	settings["current_sound_name"] = "";
	settings["video_bitrate"] = 8000;
	settings["current_video_name"] = "";
	settings["stutter_longest"] = 2;
	settings["stutter_time"] = 0.5;
	settings["audioOutId"] = 0;
	settings["audioInId"] = 1;
	settings["delay_longest"] = 1.5;
	settings["delay_time"] = 0.5;
	settings["afx_delayRecursionType"] = 0;
	settings["vfx_canny_th1"] = 50;
	settings["vfx_canny_th2"] = 150;

	settings.save("settings.json", true);
}

int ofApp::getBufferSizeIdx(int bSize) {
	int idx = 0;
	while (bSize != 64) {
		bSize /= 2;
		idx++;
	}
	return idx;
}

int ofApp::getSampleRateIdx(int sRate) {
	switch (sRate) {
	case 8000: return 0;
	case 11025: return 1;
	case 16000: return 2;
	case 22050: return 3;
	case 44100: return 4;
	case 48000: return 5;
	case 88200: return 6;
	case 96000: return 7;
	default: return 5;
	}
}

int ofApp::getSampleRate(int idx) {
	switch (idx) {
	case 0: return 8000;
	case 1: return 11025;
	case 2: return 16000;
	case 3: return 22050;
	case 4: return 44100;
	case 5: return 48000;
	case 6: return 88200;
	case 7: return 96000;
	default: return 48000;
	}
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo) {
	// ToDo: drag und drop fuer videos, die in die Galerie reinkommen sollen

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key) {
	// Right-Shift Default
	if (key == settings["hotkey_screenshot"].asInt()) {
		snapped = true;

		// screenshot prozess
		if (snapped) {
			imgScreenshot.grabScreen(0, 0, ofGetWidth(), ofGetHeight());
			imgScreenshot.save(
				settings["screenshot_path"].asString() +
				settings["screenshot_fileName"].asString() +
				(settings["screenshot_withTime"].asBool() ? ofGetTimestampString()
					: ofToString(settings["screenshot_number"].asInt(), 3, '0')) +
				settings["screenshot_format"].asString()
			);
			if (!settings["screenshot_withTime"].asBool())
				settings["screenshot_number"] = settings["screenshot_number"].asInt() + 1;
			snapped = false;
		}
	}

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key) {

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button) {

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y) {

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h) {

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg) {

}

void ofApp::exit() {
	ofSoundStreamClose();

	if (recorder.isRecording()) recorder.cancel();
	stutter_enabled = false;

	scDeleteCamera(vCamera);

	settings["vfx_canny_th1"] = vfx_canny_th1;
	settings["vfx_canny_th2"] = vfx_canny_th2;

	settings.save("settings.json", true);
}