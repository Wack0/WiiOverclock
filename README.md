# WiiOverclock

This is a fork of mini, and of babelfish, that modifies the system clock of the Wii such that any hardware that runs directly off of the system clock can be overclocked. The hardware that uses different clocks are: USB, SDMC (runs off the USB clock), VI, AI, DDR memory. Everything else runs off the system clock; the PowerPC runs at 3x system clock. 

mini is used to change the system clock and to initialise babelfish, which patches IOS in memory to attempt to fix the timing differences that changing the system clock would cause.

I have been testing on a Bollywood system (Hollywood-1; RVL-CPU-40 and later) only. Vegas support is present but completely untested. The configuration as present does nothing on Vegas (configured for the same 243MHz clock speed as normal); on Bollywood it sets a \~315MHz system clock (\~945MHz PowerPC), which is about as far as it can go whilst still being stable (\~324Mhz system clock is too unstable, at least without better cooling).

**This program is provided "as is" without warranty of any kind, either expressed or implied, including, but not limited to, the implied warranties of merchantability and fitness for a particular purpose. The entire risk as to the quality and performance of the program is with you.  Should the program prove defective, you assume the cost of all necessary servicing, repair or correction.**

(In other words: **if you cause hardware failure by overclocking your Wii, it's your own fault**. This includes if you have to get your "AI agent" to build this; binaries are currently not provided (and probably will never be) for a reason.)

## Known issues
- At some system clock speed greater than a value between ~250MHz and ~280Mhz, IOS networking will fail to work.
   - I'm not sure of the root cause; that said, the IOS socket interface is a port of the GameCube one and internally tries to convert between the gamecube clock (162MHz) and the true system clock, which may well cause issues at higher system clock values.
- I have only tested the EXI interface with a USB Gecko, other devices may have issues with a higher system clock.
- As mentioned above, I have only tested with a Bollywood system. I have not tested with a Vegas system, mainly due to the existing reports of hardware failure *without* running beyond expected hardware tolerances.
- There may be issues with low level timing in PowerPC code (official games and homebrew); the PowerPC decrementer runs off of the system clock (divided by 4), and both official code and homebrew code may have the original value hardcoded.



## Building and running

You need devkitARM installed.

To change the clock speed, change the values in `inc\clock.h` - there's no nice calculator present, but the `CLOCK_HZ` definition (different for Vegas and Bollywood) shows how to calculate the clock speed in hertz given feedback/reference/output values:

```
Vegas: 27000000 * (((CLOCK_FEEDBACK + 1) * 2) / (CLOCK_REFERENCE + 1))  / (CLOCK_OUTPUT)
Bollywood: 27000000 * (CLOCK_FEEDBACK) / (CLOCK_REFERENCE+1) / (CLOCK_OUTPUT/2) / 65536
```

- Set up DEVKITPRO= and DEVKITARM= environment variables appropriately.
- Build mini by running make in `mini/` folder.
- Build babelfish by running make in `babelfish/` folder.
  - Both mini and babelfish need to be rebuilt when changing `inc/clock.h`.
- Copy `mini/armboot.bin` to `sdmc:/bootmii/armboot.bin` and `babelfish/babelfish.bin` to `sdmc:/bootmii/bablfish.bin` (**note:** `bablfish.bin` on SD card,  not `babelfish.bin`, due to mini using old FatFs with 8.3 filename limitation!)
- Boot the armboot.bin payload using your preferred method (bootmii as IOS or some boot-time exploit; [ArmbootNow](https://github.com/InvoxiPlayGames/ArmbootNow) from Homebrew Channel).

