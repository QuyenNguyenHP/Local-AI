const $ = (id) => document.getElementById(id);
const state = { recorder: null, stream: null, chunks: [], busy: false };
const defaults = { serverUrl: "http://127.0.0.1:8000", apiKey: "", sessionId: crypto.randomUUID(), language: "vi" };

function settings() {
  return Object.fromEntries(Object.keys(defaults).map((key) => [key, $(key).value.trim()]));
}
function loadSettings() {
  const stored = JSON.parse(localStorage.getItem("voiceClientSettings") || "{}");
  for (const [key, fallback] of Object.entries(defaults)) $(key).value = stored[key] || fallback;
}
function setStatus(text) { $("recordingStatus").textContent = text; }
function setBusy(busy) { state.busy = busy; $("recordButton").disabled = busy; }

async function checkServer() {
  const url = settings().serverUrl.replace(/\/$/, "") + "/healthz";
  try {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    $("connectionStatus").textContent = "✓ Voice server đang sẵn sàng.";
  } catch (error) {
    $("connectionStatus").textContent = `Không kết nối được: ${error.message}`;
  }
}

function appendTurn(audio) {
  $("emptyState")?.remove();
  const node = $("messageTemplate").content.firstElementChild.cloneNode(true);
  const player = node.querySelector("audio");
  player.src = URL.createObjectURL(audio);
  $("conversation").append(node);
  player.play().catch(() => {});
  node.scrollIntoView({ behavior: "smooth", block: "end" });
}

async function sendAudio(file) {
  if (state.busy) return;
  const config = settings();
  if (!config.serverUrl) return setStatus("Hãy nhập URL của voice server trong Cài đặt.");
  setBusy(true); setStatus("Đang nghe, hỏi AI và tạo giọng trả lời…");
  const body = new FormData();
  body.append("audio", file, file.name || "recording.webm");
  body.append("session_id", config.sessionId || defaults.sessionId);
  body.append("language", config.language);
  // Omit response_format: server defaults to direct audio/wav (no JSON/Base64).
  try {
    const headers = config.apiKey ? { Authorization: `Bearer ${config.apiKey}` } : {};
    const response = await fetch(config.serverUrl.replace(/\/$/, "") + "/v1/voice/chat", { method: "POST", headers, body });
    if (!response.ok) {
      const data = await response.json().catch(() => ({}));
      throw new Error(data.detail || data.error || `HTTP ${response.status}`);
    }
    const audio = await response.blob();
    if (!audio.size) throw new Error("Server returned an empty audio response");
    appendTurn(audio);
    setStatus("Sẵn sàng");
  } catch (error) {
    setStatus(`Lỗi: ${error.message}`);
  } finally { setBusy(false); }
}

async function startRecording() {
  try {
    state.stream = await navigator.mediaDevices.getUserMedia({ audio: true });
    const preferred = MediaRecorder.isTypeSupported("audio/webm;codecs=opus") ? "audio/webm;codecs=opus" : "";
    state.chunks = []; state.recorder = new MediaRecorder(state.stream, preferred ? { mimeType: preferred } : undefined);
    state.recorder.ondataavailable = (event) => event.data.size && state.chunks.push(event.data);
    state.recorder.onstop = () => {
      state.stream.getTracks().forEach((track) => track.stop());
      const mimeType = state.recorder?.mimeType || "audio/webm";
      state.recorder = null;
      sendAudio(new File(state.chunks, "recording.webm", { type: mimeType }));
    };
    state.recorder.start();
    $("recordButton").classList.add("recording"); $("recordLabel").textContent = "Dừng & gửi"; setStatus("Đang ghi âm…");
  } catch (error) { setStatus(`Không truy cập được micro: ${error.message}`); }
}
function stopRecording() {
  // onstop builds the File asynchronously, so leave the recorder available
  // until the callback reads its MIME type and sends the captured audio.
  state.recorder.stop();
  $("recordButton").classList.remove("recording"); $("recordLabel").textContent = "Bắt đầu nói";
}

$("settingsButton").onclick = () => { $("settingsPanel").hidden = !$("settingsPanel").hidden; };
$("saveSettings").onclick = () => { localStorage.setItem("voiceClientSettings", JSON.stringify(settings())); checkServer(); };
$("recordButton").onclick = () => state.recorder?.state === "recording" ? stopRecording() : startRecording();
$("audioFile").onchange = (event) => { const file = event.target.files[0]; if (file) { $("fileName").textContent = file.name; sendAudio(file); } };
loadSettings(); checkServer();
