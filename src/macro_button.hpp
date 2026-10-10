// macro_button.hpp - the macro_button.png icon.
//   tap  -> opens/closes the Record / Replay / Render menu
//   drag -> moves the button (position is saved)
//   size and opacity come from the mod settings
#pragma once
#include <Geode/Geode.hpp>

#include "actions.hpp"

using namespace geode::prelude;

class MacroButton : public CCNode, public CCTouchDelegate {
protected:
    CCSprite* m_icon = nullptr;
    CCMenu* m_menu = nullptr;
    float m_size = 60.f;
    CCPoint m_touchStart;
    CCPoint m_nodeStart;
    bool m_pressed = false;
    bool m_dragging = false;

    bool init() override {
        if (!CCNode::init()) return false;

        m_size = static_cast<float>(Mod::get()->getSettingValue<double>("button-size"));
        float opacity = static_cast<float>(Mod::get()->getSettingValue<double>("button-opacity"));

        // resources/macro_button.png (500x500). Falls back to a plain button if it's missing.
        m_icon = CCSprite::create(Mod::get()->expandSpriteName("macro_button.png"));
        if (!m_icon) m_icon = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
        m_icon->setScale(m_size / m_icon->getContentSize().width);
        m_icon->setOpacity(static_cast<GLubyte>(255.f * opacity / 100.f));
        this->addChild(m_icon);

        m_menu = CCMenu::create();
        m_menu->setPosition({0.f, 0.f});
        m_menu->setVisible(false);
        this->addChild(m_menu);

        addItem("Record", menu_selector(MacroButton::onRecord), 40.f);
        addItem("Replay", menu_selector(MacroButton::onReplay), 0.f);
        addItem("Render", menu_selector(MacroButton::onRender), -40.f);

        return true;
    }

    void addItem(char const* label, SEL_MenuHandler cb, float y) {
        auto* spr = ButtonSprite::create(label);
        spr->setScale(0.7f);
        auto* item = CCMenuItemSpriteExtra::create(spr, this, cb);
        item->setPosition({0.f, y});
        m_menu->addChild(item);
    }

    void onRecord(CCObject*) { vbot::toggleRecord(); }
    void onReplay(CCObject*) { vbot::armReplay(); }
    void onRender(CCObject*) { vbot::startRender(); }

    void toggleMenu() {
        bool show = !m_menu->isVisible();
        if (show) {
            // Open towards the middle of the screen so the menu never goes off-screen.
            auto win = CCDirector::get()->getWinSize();
            float dir = this->getPositionX() > win.width / 2.f ? -1.f : 1.f;
            m_menu->setPosition({dir * (m_size / 2.f + 60.f), 0.f});
        }
        m_menu->setVisible(show);
    }

public:
    static MacroButton* create() {
        auto* ret = new MacroButton();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void onEnter() override {
        CCNode::onEnter();
        CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -600, true);
    }

    void onExit() override {
        CCDirector::get()->getTouchDispatcher()->removeDelegate(this);
        CCNode::onExit();
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (!this->isVisible()) return false;
        auto local = this->convertToNodeSpace(touch->getLocation());
        if (!m_icon->boundingBox().containsPoint(local)) return false;
        m_pressed = true;
        m_dragging = false;
        m_touchStart = touch->getLocation();
        m_nodeStart = this->getPosition();
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (!m_pressed) return;
        auto delta = ccpSub(touch->getLocation(), m_touchStart);
        if (!m_dragging && ccpLength(delta) > 10.f) m_dragging = true;
        if (m_dragging) {
            auto win = CCDirector::get()->getWinSize();
            auto p = ccpAdd(m_nodeStart, delta);
            p.x = std::clamp(p.x, 0.f, win.width);
            p.y = std::clamp(p.y, 0.f, win.height);
            this->setPosition(p);
        }
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (!m_pressed) return;
        m_pressed = false;
        if (m_dragging) {
            Mod::get()->setSavedValue<float>("macro-button-x", this->getPositionX());
            Mod::get()->setSavedValue<float>("macro-button-y", this->getPositionY());
        } else {
            this->toggleMenu();
        }
        m_dragging = false;
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override {
        m_pressed = false;
        m_dragging = false;
    }
};
