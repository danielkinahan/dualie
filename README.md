<h1 align="center">
  <br>
  <a href="https://www.danielkinahan.ca"><img src="etc/KinasonicsLogo.png" alt="KinasonicsLogo" width="200"></a>
  <br>
  Dualie
  <br>
</h1>

<h4 align="center">A polyphonic, multi-oscillator synth with many bells and many whistles. Built on the Daisy Seed platform.</h4>
 
[//]: # (<p align="center">)
[//]: # (  <a href="https://badge.fury.io/js/electron-markdownify">)
[//]: # (    <img src="https://badge.fury.io/js/electron-markdownify.svg")
[//]: # (         alt="Gitter">)
[//]: # (  </a>)
[//]: # (  <a href="https://gitter.im/amitmerchant1990/electron-markdownify"><img src="https://badges.gitter.im/amitmerchant1990/electron-markdownify.svg"></a>)
[//]: # (  <a href="https://saythanks.io/to/bullredeyes@gmail.com">)
[//]: # (      <img src="https://img.shields.io/badge/SayThanks.io-%E2%98%BC-1EAEDB.svg">)
[//]: # (  </a>)
[//]: # (  <a href="https://www.paypal.me/AmitMerchant">)
[//]: # (    <img src="https://img.shields.io/badge/$-donate-ff69b4.svg?maxAge=2592000&amp;style=flat">)
[//]: # (  </a>)
[//]: # (</p>)

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#how-to-use">How To Use</a> •
  <a href="#license">License</a>
</p>

## Key Features

* 12 voice polyphony
  - Each voice has it's own filter and amplifier envelope
* Dual oscillators with:
  - Selectable waveforms
  - Pulse width and frequency modulation
  - Coarse and fine tuning
  - Synchronicity
* Digital moog ladder filter with modulatable cutoff
* LFO with selectable waveform and frequency
  - All modulations selectable by LFO
* Selectable effects units
  - Reverb
  - Cube distortion
  - Echo
  - Chorus

## How To Use

To clone and run this application, you'll need [the Daisy development toolchain](https://github.com/electro-smith/DaisyWiki/wiki/1.-Setting-Up-Your-Development-Environment) installed on your computer. After setting up and running the blink example, from your command line:

```bash
# Clone this repository, with submodules, with fast submodule download
$ git clone --recurse-submodules -j8 https://github.com/danielkinahan/dualie

# Go into libdaisy
$ cd dualie/lib/libDaisy
# Build all the files
$ make all

# Go into daisysp and do the same
$ cd dualie/lib/DaisySP
# Build all the files
$ make all
```
Then open the command palette using ```Ctrl+P/⌘+P``` and run ```task build_and_program_dfu```

> **Note**
> If you're using Windows, make sure you have your vscode default terminal set to Git Bash.

## License

MIT

---

> [danielkinahan.ca](https://www.danielkinahan.ca) &nbsp;&middot;&nbsp;
> GitHub [@danielkinahan](https://github.com/danielkinahan)


