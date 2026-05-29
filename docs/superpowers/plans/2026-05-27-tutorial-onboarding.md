# Tutorial Onboarding Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show a 4-step spotlight tutorial the first time a user accepts the privacy policy, walking them through the settings gear, chat toggle, and keyboard shortcuts. Replayable from the settings menu.

**Architecture:** A static `s_tutorialStep` integer in `Renderer.cpp` drives a state machine. Steps 1 and 4 show a centered card with no highlight. Steps 2 and 3 capture the target element's screen rect each frame and draw a green spotlight border + dim overlay on top of the chat window. `tutorial_seen` is persisted to `FalloutChat.ini` via `main.cpp`.

**Tech Stack:** ImGui (ImDrawList for overlay, GetItemRectMin/Max for rect capture), SimpleIni (persistence), existing `##ChatSettings` popup infrastructure.

---

## File Structure

- **`src/main.cpp`** — add `g_tutorialSeen` global, `SaveTutorialSeen()` function, read `tutorial_seen` in `LoadConfigs()`
- **`src/Renderer.cpp`** — add tutorial statics, rect captures, overlay rendering function `DrawTutorialOverlay()`, trigger on privacy accept, "Replay Tutorial" button in settings popup

---

### Task 1: Add persistence to main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Add the global and SaveTutorialSeen() after SaveChatEnabled()**

In `src/main.cpp`, after the closing `}` of `SaveChatEnabled` (line 68), add:

```cpp
bool g_tutorialSeen = false;

void SaveTutorialSeen()
{
	ini.LoadFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	ini.SetBoolValue("General", "tutorial_seen", true);
	ini.SaveFile("Data\\F4SE\\Plugins\\FalloutChat.ini");
	g_tutorialSeen = true;
	ini.Reset();
}
```

- [ ] **Step 2: Read tutorial_seen in LoadConfigs()**

In `LoadConfigs()`, after the line `g_chatEnabled = ini.GetBoolValue("General", "chat_enabled", true);` (line 81), add:

```cpp
	g_tutorialSeen = ini.GetBoolValue("General", "tutorial_seen", false);
```

- [ ] **Step 3: Commit**

```
git add src/main.cpp
git commit -m "feat: add g_tutorialSeen global and SaveTutorialSeen persistence"
```

---

### Task 2: Add tutorial statics and extern declarations to Renderer.cpp

**Files:**
- Modify: `src/Renderer.cpp`

- [ ] **Step 1: Add extern declarations at the top of Renderer.cpp**

After line 27 (`extern void SaveChatEnabled(bool enabled);`), add:

```cpp
extern bool g_tutorialSeen;
extern void SaveTutorialSeen();
```

- [ ] **Step 2: Add tutorial statics after the ticker statics block**

After line 93 (`static constexpr float kTickerSpeed = 80.0f;`), add:

```cpp
		static int    s_tutorialStep = -1;        // -1=off, 1-4=active step, 5=finish
		static bool   s_tutorialForceSettings = false;
		static ImVec2 s_gearRectMin{};
		static ImVec2 s_gearRectMax{};
		static ImVec2 s_enableChatRectMin{};
		static ImVec2 s_enableChatRectMax{};
```

- [ ] **Step 3: Commit**

```
git add src/Renderer.cpp
git commit -m "feat: add tutorial statics and extern declarations"
```

---

### Task 3: Trigger tutorial after privacy policy accept

**Files:**
- Modify: `src/Renderer.cpp`

The privacy policy "I Agree & Continue" button handler is at approximately line 616:
```cpp
if (ImGui::Button("I Agree & Continue", ImVec2(200, 30))) {
    ::SavePrivacyPolicy();
}
```

- [ ] **Step 1: Set s_tutorialStep = 1 when privacy is accepted (and tutorial not yet seen)**

Replace that block with:

```cpp
if (ImGui::Button("I Agree & Continue", ImVec2(200, 30))) {
    ::SavePrivacyPolicy();
    if (!::g_tutorialSeen)
        s_tutorialStep = 1;
}
```

- [ ] **Step 2: Commit**

```
git add src/Renderer.cpp
git commit -m "feat: trigger tutorial on first privacy policy accept"
```

---

### Task 4: Capture gear button and Enable Chat checkbox rects

**Files:**
- Modify: `src/Renderer.cpp`

The gear button is drawn at approximately line 684:
```cpp
if (ImGui::Button(ICON_FA_GEAR, ImVec2(26, 0)))
    ImGui::OpenPopup("##ChatSettings");
```

The Enable Chat checkbox is drawn at approximately line 691:
```cpp
ImGui::Checkbox("Enable Chat", &g_chatEnabled);
```

- [ ] **Step 1: Capture gear rect immediately after the gear button**

After `ImGui::Button(ICON_FA_GEAR, ...)` and before `ImGui::OpenPopup`, add:

```cpp
if (ImGui::Button(ICON_FA_GEAR, ImVec2(26, 0)))
    ImGui::OpenPopup("##ChatSettings");
s_gearRectMin = ImGui::GetItemRectMin();
s_gearRectMax = ImGui::GetItemRectMax();
```

- [ ] **Step 2: Force-open settings popup on tutorial step 3**

Before the existing `if (ImGui::BeginPopup("##ChatSettings"))` line, add:

```cpp
if (s_tutorialStep == 3 && s_tutorialForceSettings) {
    ImGui::OpenPopup("##ChatSettings");
    s_tutorialForceSettings = false;
}
```

- [ ] **Step 3: Capture Enable Chat checkbox rect inside the settings popup**

After the `ImGui::Checkbox("Enable Chat", &g_chatEnabled);` line, add:

```cpp
s_enableChatRectMin = ImGui::GetItemRectMin();
s_enableChatRectMax = ImGui::GetItemRectMax();
```

- [ ] **Step 4: Commit**

```
git add src/Renderer.cpp
git commit -m "feat: capture gear and enable-chat rects for tutorial spotlight"
```

---

### Task 5: Add DrawTutorialOverlay() function

**Files:**
- Modify: `src/Renderer.cpp`

Add the full overlay function after the `SenderColor` helper (after line 77, before `static float s_passiveAlpha`). This function handles all 4 steps.

- [ ] **Step 1: Add DrawTutorialOverlay() function**

```cpp
		static void DrawTutorialOverlay()
		{
			if (s_tutorialStep < 1 || s_tutorialStep > 4)
				return;

			ImGuiIO& io      = ImGui::GetIO();
			ImVec2 dispSize  = io.DisplaySize;

			// Fullscreen transparent overlay window to draw on top of everything
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(dispSize);
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::Begin("##TutorialOverlay", nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoSavedSettings);

			ImDrawList* dl = ImGui::GetWindowDrawList();

			// Dim the whole screen
			dl->AddRectFilled(ImVec2(0, 0), dispSize, IM_COL32(0, 0, 0, 180));

			// Draw spotlight for steps that target a specific element
			ImVec2 spotMin{}, spotMax{};
			bool hasSpot = false;
			if (s_tutorialStep == 2 && s_gearRectMax.x > 0) {
				spotMin  = ImVec2(s_gearRectMin.x - 6, s_gearRectMin.y - 6);
				spotMax  = ImVec2(s_gearRectMax.x + 6, s_gearRectMax.y + 6);
				hasSpot  = true;
			} else if (s_tutorialStep == 3 && s_enableChatRectMax.x > 0) {
				spotMin  = ImVec2(s_enableChatRectMin.x - 6, s_enableChatRectMin.y - 6);
				spotMax  = ImVec2(s_enableChatRectMax.x + 6, s_enableChatRectMax.y + 6);
				hasSpot  = true;
			}

			if (hasSpot) {
				dl->AddRectFilled(spotMin, spotMax, IM_COL32(0, 255, 80, 30));
				dl->AddRect(spotMin, spotMax, IM_COL32(0, 255, 80, 255), 4.0f, 0, 3.0f);
			}

			ImGui::End();

			// Card window — centered or near spotlight
			float cardW = 400.0f;
			float cardH = (s_tutorialStep == 4) ? 160.0f : 180.0f;
			float cardX, cardY;
			if (hasSpot) {
				// Position card below the spotlight, or above if too close to bottom
				cardX = spotMin.x;
				cardY = spotMax.y + 12.0f;
				if (cardY + cardH > dispSize.y - 20.0f)
					cardY = spotMin.y - cardH - 12.0f;
				if (cardX + cardW > dispSize.x - 10.0f)
					cardX = dispSize.x - cardW - 10.0f;
			} else {
				cardX = (dispSize.x - cardW) * 0.5f;
				cardY = (dispSize.y - cardH) * 0.5f;
			}

			ImGui::SetNextWindowPos(ImVec2(cardX, cardY), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.95f);
			ImGui::Begin("##TutorialCard", nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoBringToFrontOnFocus);

			// Step indicator
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "SETUP  %d / 4", s_tutorialStep);
			ImGui::Separator();
			ImGui::Spacing();

			// Step content
			if (s_tutorialStep == 1) {
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "WELCOME TO FALLOUT 4 GLOBAL CHAT");
				ImGui::Spacing();
				ImGui::TextWrapped("This quick guide will show you the key features. Click Next to continue.");
			} else if (s_tutorialStep == 2) {
				ImGui::TextWrapped("Click the gear icon to open Chat Settings.");
			} else if (s_tutorialStep == 3) {
				ImGui::TextWrapped("Use the Enable Chat toggle to show or hide the chat overlay.");
			} else if (s_tutorialStep == 4) {
				ImGui::TextWrapped("Press F11 to open or close chat at any time.\nPress ESC to close chat quickly.");
			}

			ImGui::Spacing();
			ImGui::Spacing();

			bool advance = false;
			bool skip    = false;

			if (s_tutorialStep == 4) {
				if (ImGui::Button("Got it!", ImVec2(120, 28)))
					advance = true;
			} else {
				if (ImGui::Button("Next ->", ImVec2(100, 28)))
					advance = true;
				ImGui::SameLine();
				if (ImGui::Button("Skip", ImVec2(60, 28)))
					skip = true;
			}

			if (advance || skip) {
				if (skip || s_tutorialStep == 4) {
					::SaveTutorialSeen();
					s_tutorialStep = -1;
				} else {
					s_tutorialStep++;
					if (s_tutorialStep == 3)
						s_tutorialForceSettings = true;
				}
			}

			ImGui::End();
		}
```

- [ ] **Step 2: Commit**

```
git add src/Renderer.cpp
git commit -m "feat: add DrawTutorialOverlay() with 4-step spotlight tutorial"
```

---

### Task 6: Call DrawTutorialOverlay() each frame and add Replay button

**Files:**
- Modify: `src/Renderer.cpp`

- [ ] **Step 1: Call DrawTutorialOverlay() after the main chat window renders**

Find the end of the `if (showChat)` block — it ends with the `ImGui::End()` that closes "Fallout Multi-Chat". After that `ImGui::End()`, add:

```cpp
					DrawTutorialOverlay();
```

The area in Renderer.cpp looks like this (around line 870+):
```cpp
				ImGui::End(); // closes "Fallout Multi-Chat"

				DrawTutorialOverlay();
```

- [ ] **Step 2: Add "Replay Tutorial" button in the ##ChatSettings popup**

Inside `if (ImGui::BeginPopup("##ChatSettings"))`, after the last `ImGui::EndPopup()` separator and before `ImGui::EndPopup()`, the last items are the Website button and then EndPopup. Add the replay button after the Website button:

```cpp
				if (ImGui::Button(ICON_FA_GLOBE " Website", ImVec2(180, 0)))
					ShellExecuteA(NULL, "open", "https://fallenworld.nexus/", NULL, NULL, SW_SHOWNORMAL);
				ImGui::Separator();
				if (ImGui::Button("Replay Tutorial", ImVec2(180, 0))) {
					s_tutorialStep = 1;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
```

- [ ] **Step 3: Commit**

```
git add src/Renderer.cpp
git commit -m "feat: wire up tutorial overlay call and replay button in settings"
```

---

### Task 7: Build and verify

**Files:** none

- [ ] **Step 1: Build the DLL**

From `E:\F4SE OG\FalloutChat\`, run:
```
.\build_dll.bat
```
Expected: `Build and Deployment Successful!` with no errors.

- [ ] **Step 2: Verify tutorial_seen INI key can be reset for testing**

Open `E:\Modlists\Fallen World Alpha 2\mods\FalloutChat\F4SE\Plugins\FalloutChat.ini` and confirm the structure. To replay the tutorial in-game, set:
```ini
tutorial_seen=false
privacy_accepted=false
```
Then launch the game, open chat with F11, accept privacy policy — tutorial should begin.

- [ ] **Step 3: Commit if any last fixes were needed**

```
git add src/main.cpp src/Renderer.cpp
git commit -m "fix: tutorial onboarding post-build corrections"
git push
```
