#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <span>

#include "PocketWatch.hpp"

using namespace geode::prelude;

namespace {
    constexpr float kMaxSwayDeg = 3.f;
    constexpr float kDeepMessageInterval = 12.f;

    // Each tier is reached at the matching depth threshold; the last one repeats at full trance.
    constexpr std::array kThresholds = { 0.1f, 0.3f, 0.55f, 0.8f, 1.f };
    constexpr std::array<std::array<char const*, 3>, 5> kSuggestions = {{
        { "Look at the watch...", "Follow it with your eyes...", "Left... and right..." },
        { "Your eyelids feel heavy...", "You are getting very sleepy...", "Breathe in... and click..." },
        { "Deeper... and deeper...", "Your hands know the way...", "Let the music carry you..." },
        { "You will not miss the next orb...", "Every jump is effortless...", "You are calm. You are focused." },
        { "You are one with the level.", "There is only the next click.", "Nothing exists but the rhythm." },
    }};

    // Smallest zoom that keeps a rotated screen covering the whole window.
    float coverScale(float degrees, CCSize size) {
        float rad = std::abs(degrees) * std::numbers::pi_v<float> / 180.f;
        float aspect = std::max(size.width, size.height) / std::min(size.width, size.height);
        return std::cos(rad) + aspect * std::sin(rad);
    }
}

class $modify(HypnosisPlayLayer, PlayLayer) {
    struct Fields {
        PocketWatch* m_watch = nullptr;     // owned by the node tree
        CCLabelBMFont* m_message = nullptr; // owned by the node tree
        CCLayerColor* m_flash = nullptr;    // owned by the node tree
        float m_depth = 0.f;
        float m_sway = 0.f;
        float m_zoom = 1.f;
        float m_time = 0.f;
        float m_messageTimer = 0.f;
        size_t m_nextTier = 0;
        bool m_finished = false;
        bool m_ownsMetering = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        if (Mod::get()->getSettingValue<bool>("enabled")) this->createHypnosis();
        return true;
    }

    void createHypnosis() {
        auto fields = m_fields.self();
        auto mod = Mod::get();
        auto size = CCDirector::sharedDirector()->getWinSize();
        bool glow = mod->getSettingValue<bool>("music-glow");

        // Everything lives on the UI layer so it stays still while the level sways.
        fields->m_watch = PocketWatch::create(
            size.height * 0.36f,
            mod->getSettingValue<ccColor3B>("watch-color"),
            static_cast<GLubyte>(mod->getSettingValue<int64_t>("watch-opacity")),
            glow
        );
        fields->m_watch->setSwingSpeed(static_cast<float>(mod->getSettingValue<double>("swing-speed")));
        fields->m_watch->setPosition({ size.width / 2.f, size.height + 4.f });
        m_uiLayer->addChild(fields->m_watch, -1);

        fields->m_message = CCLabelBMFont::create("", "bigFont.fnt");
        fields->m_message->setPosition({ size.width / 2.f, size.height * 0.2f });
        fields->m_message->setColor({ 210, 200, 255 });
        fields->m_message->setOpacity(0);
        m_uiLayer->addChild(fields->m_message, 10);

        fields->m_flash = CCLayerColor::create({ 255, 255, 255, 0 });
        m_uiLayer->addChild(fields->m_flash, 11);

        auto engine = FMODAudioEngine::sharedEngine();
        if (glow && !engine->m_metering) {
            engine->enableMetering();
            fields->m_ownsMetering = true;
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto fields = m_fields.self();
        if (!fields->m_watch) return;

        fields->m_time += dt;
        fields->m_watch->setHandsTime(m_gameState.m_levelTime);

        if (!fields->m_finished && !m_player1->m_isDead) {
            auto tranceTime = static_cast<float>(Mod::get()->getSettingValue<int64_t>("trance-time"));
            fields->m_depth = std::min(1.f, fields->m_depth + dt / tranceTime);
            this->updateSuggestions(dt);
        }
        this->updateSway(dt);
    }

    void updateSuggestions(float dt) {
        auto fields = m_fields.self();
        if (!Mod::get()->getSettingValue<bool>("suggestions")) return;

        if (fields->m_nextTier < kThresholds.size()) {
            if (fields->m_depth >= kThresholds[fields->m_nextTier]) {
                this->suggest(kSuggestions[fields->m_nextTier]);
                fields->m_nextTier++;
            }
            return;
        }
        // Fully under: keep murmuring.
        fields->m_messageTimer += dt;
        if (fields->m_messageTimer >= kDeepMessageInterval) {
            fields->m_messageTimer = 0.f;
            this->suggest(kSuggestions.back());
        }
    }

    void updateSway(float dt) {
        auto fields = m_fields.self();
        auto strength = static_cast<float>(Mod::get()->getSettingValue<double>("camera-sway"));

        // The level leans after the watch and slowly breathes in and out.
        float targetSway = kMaxSwayDeg * strength * fields->m_depth * fields->m_watch->swing();
        float breathe = 1.f + 0.03f * strength * fields->m_depth * (0.5f + 0.5f * std::sin(fields->m_time * 0.8f));
        float ease = std::min(1.f, dt * 5.f);
        fields->m_sway += (targetSway - fields->m_sway) * ease;
        fields->m_zoom += (breathe - fields->m_zoom) * ease;

        float scale = coverScale(fields->m_sway, this->getContentSize()) * fields->m_zoom;
        this->setRotation(fields->m_sway);
        this->setScale(scale);
        // Both layers are full-screen and rotate around the same center, so this cancels out exactly.
        m_uiLayer->setRotation(-fields->m_sway);
        m_uiLayer->setScale(1.f / scale);
    }

    void suggest(std::span<char const* const> lines) {
        this->showMessage(lines[std::rand() % lines.size()], 2.2f);
    }

    void showMessage(char const* text, float hold) {
        auto label = m_fields->m_message;
        label->stopAllActions();
        label->setString(text);
        label->limitLabelWidth(CCDirector::sharedDirector()->getWinSize().width * 0.9f, 0.55f, 0.2f);
        label->setOpacity(0);
        label->runAction(CCSequence::create(
            CCFadeTo::create(0.8f, 230),
            CCDelayTime::create(hold),
            CCFadeTo::create(1.2f, 0),
            nullptr
        ));
    }

    // The hypnotist snaps their fingers.
    void wakeUp() {
        auto fields = m_fields.self();
        if (!fields->m_watch) return;

        if (fields->m_depth > 0.2f) {
            fields->m_flash->stopAllActions();
            fields->m_flash->runAction(CCSequence::create(
                CCFadeTo::create(0.05f, static_cast<GLubyte>(60 + 120 * fields->m_depth)),
                CCFadeTo::create(0.4f, 0),
                nullptr
            ));
            if (Mod::get()->getSettingValue<bool>("suggestions")) this->showMessage("*snap*", 0.6f);
        }
        fields->m_depth = 0.f;
        fields->m_nextTier = 0;
        fields->m_messageTimer = 0.f;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->m_finished = false;
        this->wakeUp();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        // Noclip and the anticheat spike call this without actually killing.
        if (player && player->m_isDead) this->wakeUp();
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        auto fields = m_fields.self();
        if (!fields->m_watch) return;
        fields->m_finished = true;
        if (Mod::get()->getSettingValue<bool>("suggestions")) {
            this->showMessage("...and when the music stops, you will wake up.", 3.f);
        }
    }

    void onQuit() {
        if (m_fields->m_ownsMetering) FMODAudioEngine::sharedEngine()->disableMetering();
        PlayLayer::onQuit();
    }
};
