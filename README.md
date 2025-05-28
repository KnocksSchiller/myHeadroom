# *myHeadroom*

A webcam application built on openFrameworks. Currently works on Windows 11. Originally a student project for my bachelor thesis.

## Featuring:
- video effects,
- audio effects,
- video playback,
- video and audio stuttering,
- video recording through ffmpeg,
- virtual webcam.

Video output can be configured to external applications by selecting the created virtual camera "DirectShow Softcam".

Audio output can be configured to external applications through programs like Virtual Audio cable.

## Set up guide.
1. If you don't have **openFrameworks** yet, consider downloading it at their [official website](https://openframeworks.cc/download/), ideally the Windows build for Visual Studio.
2. Clone this very Git repository, ideally into your *"myApps"* subfolder in your openFrameworks directory to avoid complications with some plugins.
3. Install [softcam](https://github.com/tshino/softcam) on your Windows environment and follow the steps mentioned in its readme file. I recommend trying it out with the example project with the combination of an app that requires a video device.
4. Import this repository's project with openFrameworks' project generator, **make sure you also have all the necessary plugins installed** (see [Plugins](#Plugins))
5. Open the project with Visual Studio and go to the project settings. Navigate to `Linker -> Input`. Edit the tab `Additional Dependencies` and add `softcam.lib` into the text field.

Make sure you installed the packages such as **OpenCV** and **JsonCpp** to your environment. I simply added them through **vcpkg** ([another guide of said application](https://youtu.be/0h1lC3QHLHU))

Feel free to contact me, if you find any isssues or whether I overlooked any steps. The initial setup of this project took several tries and got more complex during its development (sorry for that).

## Plugins
To get the project up and running, you have to install the following plugins into your *"addons"* subfolder inside your openFrameworks directory.
- ofxOpenCv (already pre-installed)
- [ofxCv](https://github.com/kylemcdonald/ofxCv)
- [ofxFastFboReader](https://github.com/satoruhiga/ofxFastFboReader)
- [ofxFFmpegRecorder](https://github.com/Furkanzmc/ofxFFmpegRecorder)
- [ofxImGui](https://github.com/jvcleave/ofxImGui/tree/develop) (use the develop branch for this!)
- [ofxJSON](https://github.com/jeffcrouse/ofxJSON)

## Project status
Currently put on ice, maybe fixing the Windows 10 issues when I find a solution to its problems. Feel free to message me if you have any questions.
