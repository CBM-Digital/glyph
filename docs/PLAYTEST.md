# First-slice playtest

Status: protocol ready; **zero external sessions recorded**. This is a test plan, not evidence of results.

## Setup

Recruit 20 players from the intended audience of short-session arcade and skill games. Include people who have never seen Glyph. Alternate which game they try first; split keyboard/mouse and controller sessions where practical. Use the same build/content revision and a fresh profile per person. Do not change controls halfway through the cohort without separating the results by build.

Run `build/glyph --demo` with `GLYPH_PROFILE_PATH` pointing to a new participant-specific local file. Keep names/contact information outside the game; use anonymous IDs P01–P20. The app does not collect or transmit telemetry. A moderator records observations manually. Ask separately before making any recording.

Before sessions, check sound, physical controller reconnection, window focus pause/resume, restart, and saved records on the actual machine. Use headphones or a normal speaker volume. Tell players: “Explore these two games as you normally would. You can stop whenever you want.” Avoid explaining mechanics during the first 60 seconds.

## Per game, 8–12 minutes

1. Start a timer on the first briefing. Observe whether the player discovers aim/launch/brake or steering/gates/jump. Note exactly where they hesitate, click, or misunderstand.
2. At 60 seconds, record whether they can intentionally perform the basic action. A lucky completion does not count as understanding.
3. After an early loss, ask: “What happened, and what would you try differently?” Record their explanation verbatim when possible. Do not suggest the answer.
4. Allow further play without asking them to retry. Count voluntary new attempts, game changes, and stops. A route's next delivery is progression, not a new attempt; an Alpine restart or Comet retry/new route is an attempt.
5. At the end, ask which moment felt best, which felt unfair or unclear, and whether they wanted another attempt. Avoid asking only whether they “liked” it.
6. Inspect the result card and saved record together only after observations are complete. For a subset, quit between Comet deliveries and verify checkpoint resume on restart.

## Record for each participant

Copy this block per participant; leave missing values blank rather than inferring them.

```text
Participant ID / build revision / date:
Hardware / OS / input device:
Game / order:
Basic action understood by 60 seconds? Yes / No / unclear
Time to first intentional action:
Voluntary attempts:
Reason for each failure (player's words):
Proposed next-attempt change (player's words):
Best moment:
Unclear/unfair moment:
Stopped because:
Control / display / audio / save defect and reproduction:
Moderator coaching given (if any, with timestamp):
```

## Internal decision targets

- At least 16 of 20 understand the basic action within 60 seconds without coaching.
- At least 12 of 20 voluntarily start three attempts.
- A majority can accurately explain their loss and describe an actionable change. Report the numerator and examples; do not substitute a vague “most enjoyed it.”

Report these per game, with raw counts and control-device split. They are proposed internal gates, not industry benchmarks or statistical proof of market demand. If a target is missed, fix the dominant control/feedback/decision problem, test with fresh players, and hold content expansion. Preserve negative findings. Do not reinterpret coached attempts as success.

After this gate, separately test demand through the finished demo, store engagement, wishlists and actual receipts. This session protocol does not validate a price or predict profitability.
