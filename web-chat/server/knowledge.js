import { execFile } from "node:child_process";
import { fileURLToPath } from "node:url";

const bridge = fileURLToPath(new URL("./knowledge_bridge.py", import.meta.url));
export function buildKnowledgeContext(question) {
  return new Promise((resolve, reject) => {
    const child = execFile(
      "python3",
      [bridge],
      { timeout: 10000, maxBuffer: 1024 * 1024 },
      (error, stdout) => {
        if (error)
          return reject(
            new Error(
              "Knowledge lookup failed. Check knowledge_rules.json, matched Markdown files, and Python 3.10+.",
            ),
          );
        try {
          resolve(JSON.parse(stdout));
        } catch {
          reject(new Error("Knowledge lookup returned an invalid result."));
        }
      },
    );
    child.stdin.on("error", () => {});
    child.stdin.end(JSON.stringify(question));
  });
}
