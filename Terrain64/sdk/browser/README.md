# Local renderer and audio input runtime

Electron 45.3.3 is bundled from official archives, pinned by SHA256 in
`sdk/browser_runtime.py`. No npm dependencies are used. The app loads only
the suite's `visualizer:` protocol. Renderer windows use Chromium sandboxing,
context isolation and no Node integration. Network requests and new windows
are blocked. The main process alone loads the N-API PCM bridge.

Recording inputs use getUserMedia/AudioWorklet. Permission is limited to audio
in the capture window. Input selection can temporarily open the default input
to obtain device labels; those temporary tracks are stopped immediately.
No recordings or diagnostic reports are uploaded. Ctrl-L reopens the input
list for standalone windows; player-owned windows continue using player PCM.

An audio packet must be acknowledged before another packet is sent. Native
callbacks catch C++ exceptions before returning to the Node API. Each renderer
awaits its GPU queue before another frame is submitted. Logs retain one 8 MiB
previous file. Closing a renderer destroys its recording broker and releases
native mappings. Player process exit also closes the associated windows.

Browser runtime license files are included unchanged. Vendored Node API
headers originate from Electron's official 45.3.3 headers archive, SHA256
7ea784acf7c7570f6c16ffcffeda62e4a57e66d2e628c480623b71562f9f634f.
`native/vendor/LICENSE.node.txt` contains Node's license and notices.
