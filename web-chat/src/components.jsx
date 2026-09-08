import React from "react";
import ReactMarkdown from "react-markdown";
import remarkGfm from "remark-gfm";
import { ArrowUpRight, Check, Copy, Cpu, Sparkles } from "lucide-react";

export function BrandMark({ compact = false }) {
  return (
    <span className={compact ? "tiny-mark" : "brand-mark"}>
      <img src="/logo/logo.png" alt={compact ? "" : "DQ logo"} />
    </span>
  );
}

export function WelcomeScreen({ suggestions, onSuggestion }) {
  return (
    <section className="welcome">
      <img className="welcome-logo" src="/logo/logo.png" alt="DQ logo" />
      <div className="eyebrow">
        <span /> A LITTLE CURIOSITY GOES A LONG WAY
      </div>
      <h1>Where should we start?</h1>
      <p>
        A thought, a question, a big idea.
        <br />
        Make room for whatever’s on your mind.
      </p>
      <div className="suggestions">
        {suggestions.map(({ icon: Icon, title, text, prompt }) => (
          <button key={title} onClick={() => onSuggestion(prompt)}>
            <Icon size={21} />
            <ArrowUpRight className="suggestion-arrow" size={15} />
            <strong>{title}</strong>
            <span>{text}</span>
          </button>
        ))}
      </div>
      <div className="welcome-foot">
        <Cpu size={14} /> Powered by your local models. Made for your ideas.
      </div>
    </section>
  );
}

export function MessageList({ messages, bottomRef, copied, onCopy }) {
  return (
    <div className="messages">
      {messages.map((message) => (
        <article className={`message ${message.role}`} key={message.id}>
          {message.role === "assistant" && (
            <div className="assistant-label">
              <BrandMark compact /> DQ AI
            </div>
          )}
          <div className="message-content">
            {message.content ? (
              <ReactMarkdown remarkPlugins={[remarkGfm]}>
                {message.content}
              </ReactMarkdown>
            ) : (
              <span className="thinking">
                Thinking<span>•••</span>
              </span>
            )}
          </div>
          {message.role === "assistant" && message.content && (
            <button
              className="copy-button"
              onClick={() => onCopy(message)}
              aria-label="Copy response"
            >
              {copied === message.id ? <Check size={15} /> : <Copy size={15} />}{" "}
              {copied === message.id ? "Copied" : "Copy"}
            </button>
          )}
        </article>
      ))}
      <div ref={bottomRef} />
    </div>
  );
}
