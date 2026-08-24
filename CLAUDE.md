# fj — notes for Claude Code

fj is a personal project: ultimately a keyboard-only (home-row f/j)
computer/OS/app platform; currently a cross-platform (Windows/Linux/web)
app implementing a digital index-card / Zettelkasten-style note system
(cards, card stacks, TOCs, threads), navigated via home-row keys.

See `PLAN.md` for the full design (card/thread model, keyboard mapping,
TODO list) and the "ARCHITECTURE" section for the current direction:
moving off Qt to a fully self-contained, zero-third-party-dependency
implementation (native OS APIs only — Win32, Xlib, browser Canvas via
Emscripten — plus a custom bitmap font/rasterizer instead of any OS
text API). Core/platform-shell split; Win32 is being built first.

## Why this project exists

This isn't just about shipping fj. In priority-agnostic order, it's
also meant to:

1. Build a fun, genuinely unique computer/app.
2. Be a vehicle for getting better at working with Claude/Claude Code.
3. Become a tool the user actually uses, eventually with educational
   features to share with their kid.
4. Serve as a portfolio piece demonstrating capability.
5. Build fluency in advanced/modern C++.
6. Build fluency in CMake.
7. Learn to set up a project that's reliable (tests, CI, build
   hygiene).
8. Learn to manage a project start to finish.
9. Keep the user's own coding ability sharp — they specifically do not
   want to atrophy into a "manager who only reviews AI output."

Goal 9 is a hard constraint on how you (Claude) should behave, not
just a background fact: don't optimize purely for "get the feature
done fastest" if that means the user stops writing/understanding the
code. See "Working style" below for what that means in practice.

## How Claude should help (beyond just writing code)

Because "learn how to best use Claude" and "learn how to manage a
project reliably" are explicit goals here, you should actively look
for and flag process improvements, not just do what's asked:

- If a request would go faster or more reliably with a different
  setup — e.g. "add this to PLAN.md so I don't have to repeat it",
  "this belongs in CLAUDE.md, not a one-off instruction", "a CMake
  preset would remove this manual step" — say so explicitly, including
  the concrete edit (which file, what to add), before just doing the
  one-off version.
- If a task is a good fit for a different tool/workflow than the
  default (e.g. planning mode for an architectural change, a subagent
  for a large search, a skill that already exists), point that out.
- On model/token usage: mention when a task is a poor fit for the
  current model tier (e.g. large mechanical/boilerplate work that
  would burn premium tokens for no quality benefit, or conversely a
  hard architectural decision that deserves more reasoning effort than
  the default). Don't switch models unilaterally — the user picks —
  but flag the tradeoff so they can decide.
- Push back visibly, don't just silently comply, when a request
  conflicts with one of the goals above (most often goal 9: don't hand
  back a fully-generated architectural solution when the point was for
  the user to design it).

## Working style

- This is a learning project as much as a build — the goal is modern
  C++ (C++23) fluency and staying sharp, not just working code.
- For core interfaces/architecture (e.g. the core/platform-shell
  contract), prefer critique over generation: point out trade-offs,
  raise the C++ idiom/reasoning, but let the human draft the first cut
  of anything architecturally significant.
- Mechanical/boilerplate work (platform shell plumbing once an
  interface is settled, CMake updates, build scripts) is fine to
  generate directly.
- When introducing a modern C++ feature, briefly explain why it's the
  modern idiom, not just use it silently.