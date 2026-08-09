import { readFileSync, existsSync } from "node:fs";
import { resolve } from "node:path";
import { z } from "zod";

const ConfigSchema = z
  .object({
    port: z.number().int().min(1).max(65535).default(5390),
    bind: z.string().default("0.0.0.0"),
    foregroundPollMs: z.number().int().min(100).default(500),
    tokenTtlHours: z.number().int().min(1).default(24),
    helperDir: z.string().default("data/helper"),
  })
  .strict();

export type ManusConfig = z.infer<typeof ConfigSchema> & { rootDir: string };

/** manus.config.json (任意) を読み、env override (MANUS_PORT / MANUS_BIND) を適用する。 */
export function loadConfig(rootDir: string): ManusConfig {
  const file = resolve(rootDir, "manus.config.json");
  let raw: unknown = {};
  if (existsSync(file)) {
    raw = JSON.parse(readFileSync(file, "utf8"));
  }
  const parsed = ConfigSchema.parse(raw);
  if (process.env.MANUS_PORT) parsed.port = Number(process.env.MANUS_PORT);
  if (process.env.MANUS_BIND) parsed.bind = process.env.MANUS_BIND;
  return { ...parsed, rootDir };
}
