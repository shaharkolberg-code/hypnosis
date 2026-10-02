#pragma once

#include <Geode/Geode.hpp>

// A pocket watch hanging from its chain, swinging like a pendulum.
// The node's position is the pivot the chain hangs from.
class PocketWatch : public cocos2d::CCNode {
public:
    static PocketWatch* create(float chainLength, cocos2d::ccColor3B color, GLubyte opacity, bool musicGlow);

    void update(float dt) override;

    void setSwingSpeed(float speed) { m_swingSpeed = speed; }
    // Rotates the hands; one minute-hand lap per minute of level time.
    void setHandsTime(double seconds);
    // Current swing position, -1 (far left) to 1 (far right).
    float swing() const;

private:
    bool init(float chainLength, cocos2d::ccColor3B color, GLubyte opacity, bool musicGlow);
    cocos2d::CCDrawNode* makeDrawNode();

    cocos2d::CCNode* m_pendulum = nullptr;
    cocos2d::CCDrawNode* m_glow = nullptr;
    cocos2d::CCDrawNode* m_minuteHand = nullptr;
    cocos2d::CCDrawNode* m_hourHand = nullptr;
    bool m_musicGlow = true;
    float m_swingSpeed = 1.f;
    float m_phase = 0.f;
    float m_pulse = 0.f;
};
