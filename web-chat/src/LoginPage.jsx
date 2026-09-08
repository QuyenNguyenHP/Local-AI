import { useState } from "react";
import { ArrowRight, ShieldCheck } from "lucide-react";

const LOGIN_PASSWORD = "Duyquyen1301@";

export default function LoginPage({ onLogin }) {
  const [username, setUsername] = useState("admin");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [submitting, setSubmitting] = useState(false);

  function submit(event) {
    event.preventDefault();
    setError("");
    if (!username.trim() || !password.trim()) {
      setError("Enter your username and password.");
      return;
    }
    if (password !== LOGIN_PASSWORD) {
      setError("Incorrect password. Please try again.");
      return;
    }
    setSubmitting(true);
    window.setTimeout(() => {
      window.localStorage.setItem("dq-ai-session", username.trim());
      onLogin(username.trim());
      setSubmitting(false);
    }, 250);
  }

  return (
    <main className="login-page">
      <section className="login-layout">
        <div className="login-intro">
          <img src="/logo/logo.png" alt="DQ AI logo" />
          <div className="eyebrow"><span /> PRIVATE LOCAL INTELLIGENCE</div>
          <h1>Your ideas,<br />your space.</h1>
          <p>Sign in to continue to DQ AI, your private workspace for thinking, building, and creating with local models.</p>
        </div>
        <section className="login-card">
          <div className="login-card-icon"><ShieldCheck size={21} /></div>
          <span className="login-badge">WELCOME BACK</span>
          <h2>Sign in to DQ AI</h2>
          <p className="login-subtitle">Continue to your personal workspace.</p>
          <form onSubmit={submit}>
            <label>Username<input value={username} onChange={(event) => setUsername(event.target.value)} autoComplete="username" required /></label>
            <label>Password<input value={password} onChange={(event) => setPassword(event.target.value)} type="password" autoComplete="current-password" required /></label>
            {error && <p className="login-error" role="alert">{error}</p>}
            <button className="login-submit" type="submit" disabled={submitting}>{submitting ? "Signing in…" : "Sign in"} <ArrowRight size={17} /></button>
          </form>
          <p className="login-note">Your conversations stay in this browser.</p>
        </section>
      </section>
    </main>
  );
}
