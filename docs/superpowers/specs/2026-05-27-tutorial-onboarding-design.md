# Tutorial Onboarding Design

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Show a 4-step spotlight tutorial the first time a user accepts the privacy policy, teaching them the key UI elements. Replayable from the settings menu.

**Architecture:** A state machine driven by a `s_tutorialStep` integer stored in `Renderer.cpp`. Each frame, if a tutorial step is active, an overlay is drawn on top of the chat window using ImDrawList. The tutorial state (`tutorial_seen`) is persisted to `FalloutChat.ini`.

**Tech Stack:** ImGui (ImDrawList for overlay), SimpleIni for persistence, existing `##ChatSettings` popup infrastructure.

---

## Trigger & Persistence

- After the user clicks "I Agree & Continue" on the privacy policy, `s_tutorialStep` is set to `1` (instead of staying at `0`).
- `tutorial_seen` boolean is saved to `FalloutChat.ini` under `[General]`.
- On load, if `tutorial_seen = true`, `s_tutorialStep` stays `-1` (inactive).
- A "Replay Tutorial" button is added to the `##ChatSettings` popup. Clicking it sets `s_tutorialStep = 1`.

`s_tutorialStep` values:
- `-1` — inactive (never draw overlay)
- `1` — Step 1: Welcome
- `2` — Step 2: Settings gear
- `3` — Step 3: Enable Chat toggle (settings popup forced open)
- `4` — Step 4: Keyboard shortcuts
- `5` — Done (save `tutorial_seen = true`, set to `-1`)

---

## Overlay Rendering (Option A — Dimmed Cutout)

Each step is rendered after the main chat window is drawn, using a fullscreen ImGui overlay window (`ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground` on a maximized window).

**Dim layer:** `dl->AddRectFilled(ImVec2(0,0), displaySize, IM_COL32(0,0,0,180))` covers the full screen.

**Spotlight cutout:** For steps that highlight an element, a bright rectangle is drawn over the dim layer using `dl->AddRectFilled(spotlightMin, spotlightMax, IM_COL32(255,255,255,0))` — but since we can't truly cut through, we use a `dl->AddRect` glowing border (4px, green `IM_COL32(0,255,80,255)`) around the target element, with an additional bright filled rect at reduced opacity (`IM_COL32(0,255,80,30)`) as a tint.

**Card:** A centered (or near-target) `ImGui::BeginChild` card with dark background, step text, and Next / Skip buttons.

---

## Steps Detail

### Step 1 — Welcome (no highlight)
- No spotlight element.
- Centered card, 400×180px.
- Title: `WELCOME TO FALLOUT 4 GLOBAL CHAT`
- Body: `"This quick guide will show you the key features. Click Next to continue."`
- Buttons: `Next →` | `Skip`

### Step 2 — Settings Gear
- Spotlight target: the gear icon button (`ICON_FA_GEAR`) in the chat toolbar. Its screen rect is captured each frame into `s_gearRect` using `ImGui::GetItemRectMin()` / `GetItemRectMax()` immediately after the button is drawn.
- Card positioned below the spotlight rect.
- Body: `"Click the gear icon to open Chat Settings."`
- Buttons: `Next →` | `Skip`

### Step 3 — Enable Chat Toggle
- The `##ChatSettings` popup is forced open this frame by calling `ImGui::OpenPopup("##ChatSettings")` before `BeginPopup`. A flag `s_tutorialForceSettings` controls this.
- Spotlight target: the Enable Chat checkbox rect, captured into `s_enableChatRect` using `GetItemRectMin/Max` after the checkbox is drawn.
- Card positioned to the right of the spotlight rect.
- Body: `"Use this toggle to show or hide the chat overlay."`
- Buttons: `Next →` | `Skip`

### Step 4 — Keyboard Shortcuts (no highlight)
- No spotlight element.
- Centered card, 400×160px.
- Body: `"Press F11 to open or close chat at any time.\nPress ESC to close chat quickly."`
- Buttons: `Got it!`
- Clicking "Got it!" advances to step 5 (saves + deactivates).

---

## Saving

`SaveTutorialSeen()` function mirrors `SavePrivacyPolicy()`:
```cpp
void SaveTutorialSeen()
{
    ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
    ini.SetBoolValue("General", "tutorial_seen", true);
    ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
    ini.Reset();
}
```

`LoadConfigs()` reads:
```cpp
bool g_tutorialSeen = ini.GetBoolValue("General", "tutorial_seen", false);
```

---

## Files Changed

- `src/main.cpp` — add `g_tutorialSeen` global, `SaveTutorialSeen()`, load from INI
- `src/Renderer.cpp` — add tutorial state machine, overlay rendering, "Replay Tutorial" button in settings popup, rect capture for gear and toggle
