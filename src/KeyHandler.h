#pragma once
#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <cstdint>

using KeyCallback     = std::function<void()>;
using KeyHandlerEvent = uint64_t;
inline constexpr KeyHandlerEvent INVALID_REGISTRATION_HANDLE = 0;
enum class KeyEventType : uint8_t { KEY_DOWN, KEY_UP };

struct KeyCallbackInfo { uint32_t key = 0; KeyEventType type = KeyEventType::KEY_DOWN; };

class KeyHandler : public RE::BSInputEventUser {
public:
    static KeyHandler* GetSingleton();
    static void RegisterSink();
    [[nodiscard]] KeyHandlerEvent Register(uint32_t bsButtonCode, KeyEventType eventType, KeyCallback callback);
    void Unregister(KeyHandlerEvent handle);
    bool ShouldHandleEvent(const RE::InputEvent* a_event) override;
    void OnButtonEvent(const RE::ButtonEvent* a_event) override;
private:
    KeyHandler() = default;
    ~KeyHandler() override = default;
    KeyHandler(const KeyHandler&) = delete;
    KeyHandler& operator=(const KeyHandler&) = delete;
    struct KeyCallbacks {
        std::map<KeyHandlerEvent, KeyCallback> down;
        std::map<KeyHandlerEvent, KeyCallback> up;
    };
    std::map<uint32_t, KeyCallbacks>           _registeredCallbacks;
    std::map<KeyHandlerEvent, KeyCallbackInfo> _handleMap;
    std::atomic<KeyHandlerEvent>               _nextHandle{ INVALID_REGISTRATION_HANDLE + 1 };
    std::shared_mutex                          _mutex;
};
