#include "PocketWatch.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace geode::prelude;

namespace {
    constexpr float kRadius = 30.f;
    constexpr float kMaxSwingDeg = 30.f;
    constexpr float kSwingPeriod = 2.4f; // seconds per full left-right-left cycle at speed 1

    ccColor4F withAlpha(ccColor3B c, float alpha, float shade = 1.f) {
        return { c.r / 255.f * shade, c.g / 255.f * shade, c.b / 255.f * shade, alpha };
    }

    CCPoint polar(float radius, float angle) {
        return { radius * std::sin(angle), radius * std::cos(angle) };
    }
}

PocketWatch* PocketWatch::create(float chainLength, ccColor3B color, GLubyte opacity, bool musicGlow) {
    auto ret = new PocketWatch();
    if (ret->init(chainLength, color, opacity, musicGlow)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

CCDrawNode* PocketWatch::makeDrawNode() {
    auto node = CCDrawNode::create();
    // RobTop's draw nodes cull against a screen-space area, which breaks once rotated.
    node->m_bUseArea = false;
    // Colors here are not premultiplied.
    node->setBlendFunc({ GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA });
    return node;
}

bool PocketWatch::init(float chainLength, ccColor3B color, GLubyte opacity, bool musicGlow) {
    if (!CCNode::init()) return false;
    m_musicGlow = musicGlow;

    float a = opacity / 255.f;
    auto gold = withAlpha(color, a);
    auto darkGold = withAlpha(color, a, 0.6f);
    auto face = ccColor4F{ 1.f, 0.97f, 0.9f, a };
    auto ink = ccColor4F{ 0.15f, 0.12f, 0.1f, a };
    auto clear = ccColor4F{ 0.f, 0.f, 0.f, 0.f };
    CCPoint center{ 0.f, -chainLength };

    m_pendulum = CCNode::create();
    this->addChild(m_pendulum);

    m_glow = this->makeDrawNode();
    m_glow->drawDot({ 0.f, 0.f }, kRadius * 1.7f, withAlpha(color, a * 0.12f));
    m_glow->drawDot({ 0.f, 0.f }, kRadius * 1.3f, withAlpha(color, a * 0.18f));
    m_glow->setPosition(center);
    m_glow->setVisible(musicGlow);
    m_pendulum->addChild(m_glow, -1);

    auto body = this->makeDrawNode();

    // Chain: alternating link sizes down to the bow ring on top of the case.
    float bowY = center.y + kRadius + 9.f;
    int link = 0;
    for (float y = 0.f; y > bowY + 4.f; y -= 6.f, ++link) {
        body->drawDot({ 0.f, y }, link % 2 ? 1.6f : 2.4f, link % 2 ? gold : darkGold);
    }
    body->drawCircle({ 0.f, bowY }, 5.f, clear, 2.f, gold, 24);
    body->drawRect({ -3.5f, center.y + kRadius - 1.f }, { 3.5f, center.y + kRadius + 5.f }, gold, 1.f, darkGold);

    // Case, face and hour marks.
    body->drawCircle(center, kRadius, gold, 2.f, darkGold, 48);
    body->drawCircle(center, kRadius * 0.8f, face, 1.f, darkGold, 48);
    for (int i = 0; i < 12; ++i) {
        float angle = i * std::numbers::pi_v<float> / 6.f;
        bool major = i % 3 == 0;
        body->drawSegment(
            center + polar(kRadius * (major ? 0.58f : 0.64f), angle),
            center + polar(kRadius * 0.72f, angle),
            major ? 1.1f : 0.6f, ink
        );
    }
    m_pendulum->addChild(body);

    m_hourHand = this->makeDrawNode();
    m_hourHand->drawSegment({ 0.f, 0.f }, { 0.f, kRadius * 0.38f }, 1.4f, ink);
    m_hourHand->setPosition(center);
    m_pendulum->addChild(m_hourHand, 1);

    m_minuteHand = this->makeDrawNode();
    m_minuteHand->drawSegment({ 0.f, -kRadius * 0.1f }, { 0.f, kRadius * 0.6f }, 0.8f, ink);
    m_minuteHand->setPosition(center);
    m_pendulum->addChild(m_minuteHand, 1);

    // Center pin and a glint on the glass, above the hands.
    auto glass = this->makeDrawNode();
    glass->drawDot(center, 2.f, darkGold);
    glass->drawDot(center + CCPoint{ -kRadius * 0.38f, kRadius * 0.38f }, kRadius * 0.14f, { 1.f, 1.f, 1.f, a * 0.45f });
    m_pendulum->addChild(glass, 2);

    this->scheduleUpdate();
    return true;
}

void PocketWatch::update(float dt) {
    m_phase = std::fmod(m_phase + dt * m_swingSpeed / kSwingPeriod, 1.f);
    m_pendulum->setRotation(-kMaxSwingDeg * this->swing());

    if (m_musicGlow) {
        // m_pulse1 sits around 0.1 in silence and rises with the music's peak level.
        float target = std::clamp(FMODAudioEngine::sharedEngine()->m_pulse1 - 0.1f, 0.f, 1.f);
        // Fast attack, slow release, so beats read as flashes rather than jitter.
        float rate = target > m_pulse ? 20.f : 4.f;
        m_pulse += (target - m_pulse) * std::min(1.f, dt * rate);
        m_glow->setScale(0.8f + 0.6f * m_pulse);
    }
}

void PocketWatch::setHandsTime(double seconds) {
    m_minuteHand->setRotation(static_cast<float>(std::fmod(seconds * 6.0, 360.0)));
    m_hourHand->setRotation(static_cast<float>(std::fmod(seconds * 0.5, 360.0)));
}

float PocketWatch::swing() const {
    return std::sin(m_phase * 2.f * std::numbers::pi_v<float>);
}
