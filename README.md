**Garden of the Wads**

Animation states:

0)
    - Glow or pulse near sensors. Occasional LED tracer
    - Wadsworth sighs
    - Venus closed

1)
    - *The initial state on startup*
    - Venus opens
    - The blue blob animation starts
    - Wadsworth breathes slowly, with pauses

2)
    - High energy tracers?
    - More colorful blobs?
    - Fast breathing?

3)
    - Mostly glowing white?
    - Rapid decay to state 2


TODOs:
- Doc PIR sensors and behavior
- The LED patterns are from old projects and need to be refactored
- Garden state updates from the Gardener are currently ignored


*Wadsworth*

*The Venus Hippy Trap*

*The Little Wads*

*Maki's Squishy Thing*

*The Garden server*

*NOTES*

For the Pi 4, need to disable wireless power saving:

```
echo iwconfig wlan0 power off > /etc/rc.local
```

The Arduino core: |The core: https://github.com/earlephilhower/arduino-pico
