import React, { useEffect, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import {
  ArrowUp,
  Square,
  Plus,
  PanelLeftClose,
  PanelLeft,
  Search,
  MessageSquare,
  Trash2,
  Sparkles,
  Code2,
  Lightbulb,
  PenLine,
  X,
  RefreshCw,
} from "lucide-react";
import { BrandMark, MessageList, WelcomeScreen } from "./components";
import LoginPage from "./LoginPage";
import "./styles.css";
const KEY = "local-ai-conversations-v1";
function readChats() {
  try {
    const value = JSON.parse(localStorage.getItem(KEY) || "[]");
    return Array.isArray(value)
      ? value.filter(
          (c) =>
            typeof c.id === "string" &&
            typeof c.title === "string" &&
            Array.isArray(c.messages) &&
            c.messages.every(
              (m) =>
                typeof m.content === "string" &&
                ["user", "assistant"].includes(m.role),
            ),
        )
      : [];
  } catch {
    return [];
  }
}
function App() {
  const [session, setSession] = useState(() => window.localStorage.getItem("dq-ai-session"));
  const [chats, setChats] = useState(readChats),
    [active, setActive] = useState(null),
    [input, setInput] = useState("");
  const [models, setModels] = useState([]),
    [model, setModel] = useState(""),
    [status, setStatus] = useState("Connecting");
  const [busy, setBusy] = useState(false),
    [error, setError] = useState(""),
    [sidebar, setSidebar] = useState(window.innerWidth > 760),
    [search, setSearch] = useState(""),
    [searchOpen, setSearchOpen] = useState(false),
    [copied, setCopied] = useState(null);
  const controller = useRef(),
    bottom = useRef(),
    textarea = useRef();
  const chat = chats.find((c) => c.id === active),
    messages = chat?.messages || [];
  useEffect(() => {
    try {
      localStorage.setItem(KEY, JSON.stringify(chats));
    } catch {
      setError(
        "Browser storage is full. Delete older conversations to save new chats.",
      );
    }
  }, [chats]);
  async function loadModels() {
    setStatus("Connecting");
    try {
      const r = await fetch("/api/models");
      const data = await r.json();
      if (!r.ok) throw new Error(data.error);
      setModels(data.models);
      setModel((old) =>
        data.models.includes(old)
          ? old
          : data.models.includes(data.defaultModel)
            ? data.defaultModel
            : data.models[0] || "",
      );
      setStatus(data.models.length ? "Running locally" : "No models installed");
      if (!data.models.length)
        setError("Install a model with: ollama pull gemma3:4b");
      else setError("");
    } catch (e) {
      setStatus("Offline");
      setError(e.message);
    }
  }
  useEffect(() => {
    loadModels();
    return () => controller.current?.abort();
  }, []);
  useEffect(() => {
    bottom.current?.scrollIntoView({ behavior: busy ? "instant" : "smooth" });
  }, [messages.length, messages.at(-1)?.content]);
  useEffect(() => {
    if (textarea.current) {
      textarea.current.style.height = "auto";
      textarea.current.style.height =
        Math.min(textarea.current.scrollHeight, 180) + "px";
    }
  }, [input]);
  function newChat() {
    if (busy) return;
    setActive(null);
    setInput("");
    setError("");
    if (window.innerWidth < 760) setSidebar(false);
    textarea.current?.focus();
  }
  function updateMessage(id, messageId, content) {
    setChats((prev) =>
      prev.map((c) =>
        c.id === id
          ? {
              ...c,
              messages: c.messages.map((m) =>
                m.id === messageId ? { ...m, content } : m,
              ),
            }
          : c,
      ),
    );
  }
  async function send(event) {
    event?.preventDefault();
    const prompt = input.trim();
    if (!prompt || busy || !model) return;
    const id = active || crypto.randomUUID(),
      answerId = crypto.randomUUID();
    const history = [
      ...messages.filter((m) => m.content.trim()),
      { id: crypto.randomUUID(), role: "user", content: prompt },
    ];
    const next = [...history, { id: answerId, role: "assistant", content: "" }];
    setChats((prev) =>
      active
        ? prev.map((c) => (c.id === id ? { ...c, messages: next } : c))
        : [{ id, title: prompt.slice(0, 64), messages: next }, ...prev],
    );
    setActive(id);
    setInput("");
    setError("");
    setBusy(true);
    controller.current = new AbortController();
    let answer = "";
    try {
      const response = await fetch("/api/chat", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        signal: controller.current.signal,
        body: JSON.stringify({
          model,
          messages: history
            .slice(-40)
            .map(({ role, content }) => ({ role, content })),
        }),
      });
      if (!response.ok) {
        const data = await response.json();
        throw new Error(data.error || "Request failed");
      }
      const data = await response.json();
      answer = data.choices?.[0]?.message?.content || "";
      updateMessage(id, answerId, answer);
      if (!answer.trim())
        throw new Error(
          "The model returned an empty response. Please try again.",
        );
    } catch (e) {
      if (e.name !== "AbortError") {
        setError(e.message);
        if (!answer) setInput(prompt);
      }
    } finally {
      setBusy(false);
      controller.current = null;
      setChats((prev) =>
        prev.map((c) =>
          c.id === id
            ? {
                ...c,
                messages: c.messages.filter(
                  (m) => m.id !== answerId || m.content.trim(),
                ),
              }
            : c,
        ),
      );
    }
  }
  async function copy(m) {
    try {
      await navigator.clipboard.writeText(m.content);
      setCopied(m.id);
      setTimeout(() => setCopied(null), 1800);
    } catch {
      setError("Clipboard is unavailable in this browser.");
    }
  }
  const suggestions = [
    {
      icon: Code2,
      title: "Build something",
      text: "Turn an idea into working code",
      prompt:
        "Help me build a useful Python automation script. Ask me what I want to automate.",
    },
    {
      icon: PenLine,
      title: "Find the right words",
      text: "Write, rewrite, and make it yours",
      prompt:
        "Help me write a clear, professional email. Ask me who it is for and what I want to say.",
    },
    {
      icon: Lightbulb,
      title: "Make it click",
      text: "Understand something new",
      prompt: "Explain how large language models work using a simple analogy.",
    },
    {
      icon: Sparkles,
      title: "Explore an idea",
      text: "A fresh perspective starts here",
      prompt: "Brainstorm five creative weekend projects using a Raspberry Pi.",
    },
  ];
  if (!session) return <LoginPage onLogin={setSession} />;
  return (
    <div className="app">
      {sidebar && <div className="scrim" onClick={() => setSidebar(false)} />}
      <aside className={sidebar ? "sidebar open" : "sidebar"}>
        <div className="brand">
          <BrandMark />
          <strong>
            DQ AI<span className="brand-dot">.</span>
          </strong>
          <button
            className="icon-button collapse"
            onClick={() => setSidebar(false)}
            aria-label="Close sidebar"
          >
            <PanelLeftClose size={18} />
          </button>
        </div>
        <button className="new-chat" onClick={newChat} disabled={busy}>
          <Plus size={18} /> New chat <kbd>＋</kbd>
        </button>
        <button
          className="search-toggle"
          onClick={() => setSearchOpen(!searchOpen)}
        >
          <Search size={17} /> Search conversations
        </button>
        {searchOpen && (
          <input
            autoFocus
            className="search-input"
            placeholder="Search chats…"
            aria-label="Search conversations"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
          />
        )}
        <div className="history-label">YOUR CONVERSATIONS</div>
        <nav className="history">
          {chats
            .filter((c) => c.title.toLowerCase().includes(search.toLowerCase()))
            .map((c) => (
              <div
                className={"history-row " + (active === c.id ? "selected" : "")}
                key={c.id}
              >
                <button
                  disabled={busy}
                  onClick={() => {
                    setActive(c.id);
                    setError("");
                    if (window.innerWidth < 760) setSidebar(false);
                  }}
                >
                  <MessageSquare size={15} />
                  <span>{c.title}</span>
                </button>
                <button
                  disabled={busy}
                  className="delete"
                  aria-label={"Delete " + c.title}
                  onClick={() => {
                    setChats((prev) => prev.filter((x) => x.id !== c.id));
                    if (active === c.id) setActive(null);
                  }}
                >
                  <Trash2 size={14} />
                </button>
              </div>
            ))}
          {!chats.length && (
            <p className="empty-history">
              A little space for your big ideas.
              <br />
              Your conversations will appear here.
            </p>
          )}
        </nav>
        <div className="local-card">
          <div>
            <span
              className={
                "status-dot " + (status === "Running locally" ? "" : "offline")
              }
            />
            <strong>{status}</strong>
            <button
              className="icon-button"
              onClick={loadModels}
              aria-label="Refresh connection"
            >
              <RefreshCw size={13} />
            </button>
          </div>
          <p>Your ideas. Your machine.</p>
        </div>
        <div className="profile">
          <span className="avatar">Y</span>
          <div>
            <strong>Your workspace</strong>
            <small>Personal · Local storage</small>
          </div>
          <span className="free-tag">LOCAL</span>
        </div>
      </aside>
      <main>
        <header>
          {!sidebar && (
            <button
              className="icon-button"
              onClick={() => setSidebar(true)}
              aria-label="Open sidebar"
            >
              <PanelLeft size={20} />
            </button>
          )}
          <div className="model-control">
            <select
              aria-label="Choose AI model"
              value={model}
              onChange={(e) => setModel(e.target.value)}
              disabled={busy || !models.length}
            >
              {!models.length && <option value="">DQ AI</option>}
              {models.map((m) => (
                <option key={m} value={m}>
                  {m.replace(":latest", "")}
                </option>
              ))}
            </select>
            <span>LOCAL MODEL</span>
          </div>
          <div className="header-note">
            <span className="status-dot" />
            Private by design
          </div>
        </header>
        <div
          className={"conversation " + (!messages.length ? "welcome-mode" : "")}
        >
          {!messages.length ? (
            <WelcomeScreen
              suggestions={suggestions}
              onSuggestion={(prompt) => {
                setInput(prompt);
                textarea.current?.focus();
              }}
            />
          ) : (
            <MessageList
              messages={messages}
              bottomRef={bottom}
              copied={copied}
              onCopy={copy}
            />
          )}
        </div>
        <div className="composer-area">
          {error && (
            <div role="alert" className="error">
              {error}
              <button
                className="icon-button"
                aria-label="Dismiss error"
                onClick={() => setError("")}
              >
                <X size={16} />
              </button>
            </div>
          )}
          <form className="composer" onSubmit={send}>
            <textarea
              ref={textarea}
              value={input}
              onChange={(e) => setInput(e.target.value)}
              onKeyDown={(e) => {
                if (
                  e.key === "Enter" &&
                  !e.shiftKey &&
                  !e.nativeEvent.isComposing
                ) {
                  e.preventDefault();
                  send();
                }
              }}
              placeholder="Ask anything, or think out loud…"
              aria-label="Message"
              rows={1}
              maxLength={32000}
            />
            <div className="composer-bottom">
              <span>
                <span className="status-dot" />{" "}
                {model ? model.replace(":latest", "") : "Connect a local model"}
              </span>
              {busy ? (
                <button
                  type="button"
                  className="send-button"
                  onClick={() => controller.current?.abort()}
                  aria-label="Stop generating"
                >
                  <Square size={16} fill="currentColor" />
                </button>
              ) : (
                <button
                  className="send-button"
                  disabled={!input.trim() || !model}
                  aria-label="Send message"
                >
                  <ArrowUp size={20} />
                </button>
              )}
            </div>
          </form>
          <p className="disclaimer">
            AI can make mistakes. Give important details a second look.
            <span>Shift + Enter for a new line</span>
          </p>
        </div>
      </main>
    </div>
  );
}
createRoot(document.getElementById("root")).render(<App />);
