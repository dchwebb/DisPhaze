# Dis Phaze
![Image](https://github.com/dchwebb/DisPhaze/raw/master/pictures/disphaze_front.png "icon")
![Image](https://github.com/dchwebb/DisPhaze/raw/master/pictures/disphaze_back.png "icon")

Eurorack module providing two Phase Distortion oscillators based on Casio's CZ synthesizors. 

Phase distortion involves warping a sine wave using a second phase distortion signal. The phase distortion tables in this module were loosely based on waveforms sampled from a Casio CZ-1000. Both oscillators allow the sine wave to be warped by a greater amount than on the original and in the case of oscillator 1 the phase distortion tables can be continuously varied.

Functionality
-------------

Disphaze is a two channel design where each channel controls 5 phase distortion waves. These are used to change the playback rate of the audio wave which is always a sine wave. All of the PD waves are based on the CZ but some are generated mathematically, some literally sampled and converted from an original CZ-1000.

The module is duophonic – channel one has a bit more range and so can go more extreme than the CZ. Channel one allows the PD waves morph from one to the next. The second channel does not morph and has a more limited range so it is a bit more 'authentic'. Channel two can also be offset by an octave from channel one which is particularly useful for ring modulation. The module works well in stereo with the two channels split left and right.

Channel one has a Ring Mod switch which digitally multiplies both channel ouputs. Channel two has a mix switch which adds both channel outputs.

The amount of phase distortion applied can be controlled with a knob and/or CV input. Pitch is applied with a 1V/Octave input which is also normalled to the Doepfer standard 16 pin Eurorack connector. The module is also supplied with a VCA input - this allows the use of an envelope generator (or other CV source) to control volume without the need for an external VCA.

Calibration
-----------

 * Set fine tune and both phase distortion amount knobs to the center position
 * Connect a voltage source to the 1V/Oct input
 * Press the calibration button once (top right of back of PCB) - a square wave will be output on channel 1
 * The top left knob will adjust the tuning offset (will be affected by spread setting)
 * The top right knob will adjust the tuning spread (test by checking different octaves)
 * When the calibration is complete press the calibration button again to save

Technical
---------

The heart of the module is the STM32F405 microcontroller running at 168MHz. Timing is generated with an external 8MHz crystal oscillator. CV inputs are conditioned with an MCP6004 OpAmp. The outputs are derived from the Microcontroller's 12 bit DACs amplified via a TL072 OpAmp. The microcontroller is programmed from a 4 pin SWD header.

The module uses a two PCB sandwich construction with vertical potentiometers and jack sockets connecting the two PCBs and providing structural rigidity.
 
 Errata
 ------
 
 * On v2 of the PCB the BOOT0 pin (pin 60) is floating which causes problems programming and debugging over the SWD interface. A bodge wire connects this to ground (pin 2 of C9).
 * Silkscreen for C4 and C17 are incorrectly swapped
 * VDDA is missing decoupling capacitors
 * Reset button missing
 * USB/UART not exposed
 * No silkscreen on calibrate button
 
 ![Image](https://github.com/dchwebb/DisPhaze/raw/master/pictures/disphaze_botch.jpg "icon")
 
