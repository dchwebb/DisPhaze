# Dis Phaze
![Image](https://github.com/dchwebb/DisPhaze/raw/master/pictures/disphaze_front.png "icon")
![Image](https://github.com/dchwebb/DisPhaze/raw/master/pictures/disphaze_back.png "icon")

Eurorack module providing two Phase Distortion oscillators based on Casio's CZ synthesizors. 

Phase distortion involves warping a sine wave using a second phase distortion signal. The phase distortion tables in this module were loosely based on waveforms sampled from a Casio CZ-1000. Both oscillators allow the sine wave to be warped by a greater amount than on the original and in the case of oscillator 1 the phase distortion tables can be continuously varied.

In keeping with the spirit of the original design the audio outputs come directly from the STM32F405 Microcontroller's 12 bit DACs. CV inputs allow control over the amount of phase distortion and the module also features a digital VCA so an envelope can be used directly to control the output level. As on the original Casio synthesizors a digital ring modulation is available which works well with the octave switch that allows oscillator 2 to vary one octave up or down in relation to oscillator 1. 
