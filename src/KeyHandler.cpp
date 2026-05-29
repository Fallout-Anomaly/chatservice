#include "KeyHandler.h"

KeyHandler* KeyHandler::GetSingleton() { static KeyHandler instance; return &instance; }

void KeyHandler::RegisterSink() {
    auto* mc = RE::MenuControls::GetSingleton();
    if (mc) mc->handlers.push_back(GetSingleton());
}

KeyHandlerEvent KeyHandler::Register(uint32_t bsButtonCode, KeyEventType eventType, KeyCallback callback) {
    std::unique_lock lock(_mutex);
    KeyHandlerEvent handle = _nextHandle++;
    _handleMap[handle] = { bsButtonCode, eventType };
    auto& cbs = _registeredCallbacks[bsButtonCode];
    if (eventType == KeyEventType::KEY_DOWN) cbs.down[handle] = std::move(callback);
    else                                     cbs.up[handle]   = std::move(callback);
    return handle;
}

void KeyHandler::Unregister(KeyHandlerEvent handle) {
    std::unique_lock lock(_mutex);
    auto it = _handleMap.find(handle);
    if (it == _handleMap.end()) return;
    auto& cbs = _registeredCallbacks[it->second.key];
    if (it->second.type == KeyEventType::KEY_DOWN) cbs.down.erase(handle);
    else                                            cbs.up.erase(handle);
    _handleMap.erase(it);
}

bool KeyHandler::ShouldHandleEvent(const RE::InputEvent* a_event) {
    return a_event &&
           a_event->eventType == RE::INPUT_EVENT_TYPE::kButton &&
           a_event->device    == RE::INPUT_DEVICE::kKeyboard;
}

void KeyHandler::OnButtonEvent(const RE::ButtonEvent* a_event) {
    if (!a_event) return;
    const uint32_t key = static_cast<uint32_t>(a_event->GetBSButtonCode());
    std::shared_lock lock(_mutex);
    auto it = _registeredCallbacks.find(key);
    if (it == _registeredCallbacks.end()) return;
    if (a_event->QJustPressed()) {
        for (auto& [h, cb] : it->second.down) cb();
    } else if (a_event->QAnalogValue() == 0.0F) {
        for (auto& [h, cb] : it->second.up) cb();
    }
}
