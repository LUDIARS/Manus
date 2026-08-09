import { z } from "zod";

const modifiers = z.array(z.enum(["ctrl", "shift", "alt", "win"])).optional();

export const ClientMessageSchema = z.discriminatedUnion("type", [
  z.object({ type: z.literal("text"), text: z.string().max(4096), seq: z.number().optional() }),
  z.object({
    type: z.literal("key"),
    key: z.string().min(1).max(16),
    modifiers,
    action: z.enum(["press", "down", "up"]).optional(),
    seq: z.number().optional(),
  }),
  z.object({
    type: z.literal("mouse.move"),
    dx: z.number(),
    dy: z.number(),
    seq: z.number().optional(),
  }),
  z.object({
    type: z.literal("mouse.button"),
    button: z.enum(["left", "right", "middle"]),
    action: z.enum(["down", "up", "click"]),
    seq: z.number().optional(),
  }),
  z.object({
    type: z.literal("mouse.wheel"),
    dx: z.number().optional(),
    dy: z.number().optional(),
    seq: z.number().optional(),
  }),
  z.object({ type: z.literal("ping"), seq: z.number().optional() }),
]);

export type ClientMessage = z.infer<typeof ClientMessageSchema>;
