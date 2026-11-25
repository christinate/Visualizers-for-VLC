/*
 * Geiss HDR
 * Copyright (c) 2026 Ryan Geiss
 * www.geisswerks.com/geiss_hdr
 *
 * License: Apache-2.0 - see /LICENSE.txt
 * 
 * Attribution Notice: see /NOTICE.txt
 *   Derivative works and redistributions must retain this NOTICE file.
 *
 * Naming / Branding Notice: see /NOTICE.txt
 *   "Geiss" and "Geiss HDR" are reserved names for the original Geiss HDR project.
 *   The Apache-2.0 license does not grant permission to use those names, or
 *   confusingly similar names, for derivative works except as needed to describe
 *   the origin of the work or reproduce the content of the NOTICE file.
 * 
 * Output permissions (for still images and still image sequences generated
 *   using this software): see /OUTPUTS.txt
 */

export class AudioInput {
  constructor({ fftSize = 2048 } = {}) {
    this.fftSize = fftSize;
    this.ctx = null;
    this.stream = null;
    this.switching_source = false;
    this.analyser = null;
    
    this.waveF = null;
    this.spectrum = null;

		this.audio_el = null;
		this.src_node = null;
		this.mode = null;
		
		this.object_url = null;	
			
		this.media_keys_hooked_up = false;
		this.prev_song_requested = false;
		this.next_song_requested = false;
  }

  // Dispose a detached graph. Capture tracks must be stopped explicitly;
  // closing an AudioContext alone does not release the microphone/tab capture.
  async _disposeSource(source) {
    if (source.audio_el) {
      source.audio_el.pause();
      source.audio_el.removeAttribute("src");
      source.audio_el.load();
    }
    for (const node of [source.src_node, source.analyser]) {
      if (node) { try { node.disconnect(); } catch (_) {} }
    }
    if (source.stream) {
      for (const track of source.stream.getTracks()) track.stop();
    }
    if (source.object_url) URL.revokeObjectURL(source.object_url);
    if (source.ctx && source.ctx.state !== "closed") {
      try { await source.ctx.close(); }
      catch (error) { console.warn("Audio context cleanup failed:", error); }
    }
  }

  async startTab() {
    return this._startSource("tab");
  }

  async startMic() {
    return this._startSource("mic");
  }

  async startMP3(url, start_volume, loop) {
    return this._startSource("mp3", url, start_volume, loop);
  }

  async _startSource(mode, url = null, start_volume = 1.0, loop = false,
                     object_url = null) {
    // Do not let overlapping permission/play promises overwrite each other's graph.
    if (this.switching_source) {
      if (object_url) URL.revokeObjectURL(object_url);
      return {success: false, error: "An audio source switch is already in progress."};
    }
    this.switching_source = true;
    const next = {ctx: null, analyser: null, src_node: null, audio_el: null,
                  stream: null, object_url, mode, waveF: null, spectrum: null};
    let committed = false;
    try {
      // Build separately so permission denial leaves the active source untouched.
      if (mode === "mic") {
        next.stream = await navigator.mediaDevices.getUserMedia({audio: true});
      } else if (mode === "tab") {
        const controller = ("CaptureController" in window) ? new CaptureController() : null;
        next.stream = await navigator.mediaDevices.getDisplayMedia({
          video: true, audio: true, ...(controller ? {controller} : {}),
        });
        if (controller?.setFocusBehavior) controller.setFocusBehavior("no-focus-change");
        for (const track of next.stream.getVideoTracks()) track.stop();
        if (!next.stream.getAudioTracks().length) {
          throw new Error("No tab audio was shared. Enable Share audio in the picker.");
        }
      }
      next.ctx = new (window.AudioContext || window.webkitAudioContext)();
      next.analyser = next.ctx.createAnalyser();
      next.analyser.fftSize = this.fftSize;
      next.analyser.smoothingTimeConstant = 0.6;
      next.analyser.minDecibels = -100;
      next.analyser.maxDecibels = -30;
      next.waveF = new Float32Array(next.analyser.fftSize);
      next.spectrum = new Float32Array(next.analyser.frequencyBinCount);
      if (mode === "mp3") {
        next.audio_el = new Audio();
        next.audio_el.src = url;
        next.audio_el.loop = loop;
        next.audio_el.preload = "auto";
        next.audio_el.volume = Math.max(0, Math.min(1, start_volume));
        next.src_node = next.ctx.createMediaElementSource(next.audio_el);
      } else {
        next.src_node = next.ctx.createMediaStreamSource(next.stream);
      }
      next.src_node.connect(next.analyser);
      await next.ctx.resume();
      // Start playback without connecting speakers yet, avoiding overlapping music.
      if (next.audio_el) await next.audio_el.play();
      if (next.audio_el) next.src_node.connect(next.ctx.destination);
      const previous = {};
      for (const key of Object.keys(next)) previous[key] = this[key];
      Object.assign(this, next);
      committed = true;
      this.prev_song_requested = false;
      this.next_song_requested = false;
      await this._disposeSource(previous);
      if (mode === "mp3") this.HookUpMediaKeys();
      return {success: true, error: ""};
    } catch (error) {
      if (!committed) await this._disposeSource(next);
      console.error(`Starting ${mode} failed:`, error);
      return {success: false, error: `Starting ${mode} failed: ${error}`};
    } finally {
      this.switching_source = false;
    }
  }

	// 'file' should be a Blob, File, or MediaSource object.
	async loadLocalFile(file, start_volume = 1.0, loop = true) {
	  if (!file) return { success : false, error : `loadLocalFile failed: no file specified` };
	
	  // Be somewhat permissive: MIME can be empty on some systems.
	  const name = file.name || "";
	  const type = file.type || "";
	  const looks_like_audio =
	    type.startsWith("audio/") ||
	    /\.mp3$/i.test(name) ||
	    /\.wav$/i.test(name) ||
	    /\.m4a$/i.test(name) ||
	    /\.ogg$/i.test(name);
	
	  if (!looks_like_audio) {
	    console.warn("loadLocalFile: not an audio file:", file);
	    return { success : false, error : "loadLocalFile failed: file ${file} is not an audio file" };
	  }
	
    const url = URL.createObjectURL(file);
    // Ownership transfers to the candidate graph, including cleanup on failure.
    return this._startSource("mp3", url, start_volume, loop, url);
  }
  

	//isLocalMode() {
	//  return this.mode === "local";
	//}
	
	setVolume(v) {
	  if (this.audio_el) {
	    this.audio_el.volume = Math.max(0, Math.min(1, v));
	  }
	}	
	
	isPaused() {
		return this.audio_el?.paused ?? true;
	}
	
	togglePause() {
		if (!this.audio_el) return false;
		if (this.audio_el.paused) {
	    this.audio_el.play();
	    return true;			
		} else {
	    this.audio_el.pause();
	    return false;
		}
	}
	
	adjustVolume(delta) {
	  if (this.audio_el) {
	    this.setVolume(this.audio_el.volume + delta);
	  }
	}

	rewindCurrentSong() {
		if (this.audio_el) this.audio_el.currentTime = 0.0;	
		//if (songHasEnded()) {
		//	await this.audio_el.play();
		//}
	}

	play() {
		return this.audio_el?.play();
	}

	songHasEnded() {
		return this.audio_el?.ended ?? false;
	}

	getCurrentSongTimeInSeconds() {
		return this.audio_el?.currentTime ?? 0;	
	}
	
	getCurrentSongLengthInSeconds() {
		return this.audio_el?.duration ?? 0;
	}
	
	seekRelative(seconds) {
		//console.log("this.audio_el = ", this.audio_el,
		//            "currentTime =", this.audio_el.currentTime,
		//            "duration =", this.audio_el.duration,
		//            "readyState =", this.audio_el.readyState);		
	  if (this.audio_el) {
	    this.audio_el.currentTime += seconds;
	  }
	}
	//seekRelative(seconds) {
	//  if (!this.audio_el) return;
	//
	//  const a = this.audio_el;
	//  const old_t = a.currentTime;
	//  let new_t = old_t + seconds;
	//  new_t = Math.max(0, Math.min(a.duration, new_t));
	//
	//  console.log("seekRelative BEFORE:", { old_t, new_t, duration: a.duration });
	//
	//  a.currentTime = new_t;
	//
	//  console.log("seekRelative AFTER set:", {
	//    currentTime: a.currentTime,
	//    duration: a.duration
	//  });
	//
	//  setTimeout(() => {
	//    console.log("seekRelative 200ms later:", {
	//      currentTime: a.currentTime,
	//      duration: a.duration
	//    });
	//  }, 200);
	//}
	
	HookUpMediaKeys() {
		if (this.media_keys_hooked_up) {
			return;
		}
		
		this.media_keys_hooked_up = true;

		if (!('mediaSession' in navigator)) return;

    //navigator.mediaSession.setActionHandler('play', async () => {
    //  await audio_el.play();
    //});
    //navigator.mediaSession.setActionHandler('pause', () => {
    //  audio_el.pause();
    //});
    navigator.mediaSession.setActionHandler('previoustrack', () => {
      if (this.mode === "mp3") this.prev_song_requested = true;
    });
    navigator.mediaSession.setActionHandler('nexttrack', () => {
      if (this.mode === "mp3") this.next_song_requested = true;
    });		
	}
	
	async loadNewSong(url) {
	  if (!this.audio_el) return;
	
	  if (this.object_url) {
      URL.revokeObjectURL(this.object_url);
      this.object_url = null;
    }
	  this.audio_el.src = url;
	  this.audio_el.currentTime = 0;
	
	  try {
	    await this.audio_el.play();
	  } catch (e) {
	    console.error("loadNewSong failed:", e);
	  }
	}

	_hzToBin(hz) {
	  const nyquist = this.ctx.sampleRate / 2;
	  const N = this.analyser.frequencyBinCount;
	  const clamped = Math.max(0, Math.min(nyquist, hz));
	  return Math.round((clamped / nyquist) * (N - 1));
	}
	
	_binToHz(bin) {
	  const nyquist = this.ctx.sampleRate / 2;
	  const N = this.analyser.frequencyBinCount;
	  return bin * (nyquist / (N - 1));
	}

	// Converts a db reading to a value in a perceptually-flat spectrum.
	_dbToPerceptual(db, freq) {
		// dB value -> linear power (not amplitude)
	  // db = 10*log10(P)  =>  P = 10^(db/10)
	  let value = Math.pow(10, db / 10);
	  
	  // Rough perceptual equalization: apply  == 1 power-law tilt.
	  value *= freq;
	  
	  return value;
	}

	// Modifies 'spectrum' to go from dB to perceptually flat,
	// and then extracts the average value in each desired band.
	NormalizeSpectrumAndSumBands(spectrum, bands) {
	  let out = new Array(4);
	  const N = spectrum.length;

		// Normalize the (entire) spectrum.
		for (let i = 0; i < N; i++) {
      let freq = this._binToHz(i);
      spectrum[i] = this._dbToPerceptual(spectrum[i], freq);
		}

		// Sum the bands.
		if (bands != null) {
			out = new Float32Array(bands.length);
			for (let b = 0; b < bands.length; b++) {
		    const i0 = this._hzToBin(bands[b].f0);
		    const i1 = this._hzToBin(bands[b].f1);
		
		    const lo = Math.max(0, Math.min(i0, i1));
		    const hi = Math.min(N - 1, Math.max(i0, i1));
		
		    let sumP = 0;
		    for (let i = lo; i <= hi; i++) {
		      sumP += spectrum[i];
		    }
		
		    // Normalize by number of bins so bands are comparable.
		    const norm = (hi >= lo) ? (hi - lo + 1) : 1;
		    out[b] = sumP / norm;
		  }
		}
	
	  return out;
	}
	
	isPrevSongRequested() {
		if (this.prev_song_requested) {
			this.prev_song_requested = false;
			return true;
		}
		return false;
	}
	isNextSongRequested() {
		if (this.next_song_requested) {
			this.next_song_requested = false;
			return true;
		}
		return false;
	}

	getFrame({ waveScale = 1.0, wantWave = true, wantSpec = true, bandsHz = null } = {}) {
	  if (!this.analyser) return null;
  
		// --- waveform ---
	  let rms = null;
		let visual_energy = null;
	  if (wantWave) {
			// float source data:
	    this.analyser.getFloatTimeDomainData(this.waveF);
	    
	    let sumSq = 0.0;
			let energy_sum = 0.0;
			let prev_v = this.waveF[0] * waveScale;
			for (let i = 0; i < this.waveF.length; i++) {
			  let v = this.waveF[i] * waveScale;          // -1..+1
			  this.waveF[i] = v;
	      sumSq += v * v;
				
				energy_sum += Math.abs(v - prev_v);
				prev_v = v;
			}
	    rms = Math.sqrt(sumSq / this.waveF.length);
			visual_energy = energy_sum / (this.waveF.length - 1) / waveScale;
	    //console.log(rms);
		}

	  // --- spectrum ---
	  let spectrum = null;
	  let bandEnergy = null;
	
	  if (wantSpec) {
	    this.analyser.getFloatFrequencyData(this.spectrum);
	    spectrum = this.spectrum;	
	    bandEnergy = this.NormalizeSpectrumAndSumBands(spectrum, bandsHz);
	  }
  
	  return {
	    wave: wantWave ? this.waveF : null,
	    rms,
	    spectrum,      // (Float32Array)
	    bandEnergy,    // {bass, mid, treble} or whatever bandsHz names you pass
			visual_energy     // A basic measure of the (visual) energy at all frequencies, combined, from the time domain signal only.
	  };
  }
}

