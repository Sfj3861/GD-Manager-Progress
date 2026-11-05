#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
using namespace geode::prelude;
namespace {
class ProgressPanel final : public FLAlertLayer {
protected:
 bool initForLevel(GJGameLevel* level) {
  if (!FLAlertLayer::init(300.f, 190.f, "GD Manager Progress", "GD Manager Progress", "Close", nullptr, 300.f)) return false;
  auto title = CCLabelBMFont::create(level ? level->m_levelName.c_str() : "Selected level", "bigFont.fnt");
  title->setScale(0.45f); title->setPosition({150.f,145.f}); m_mainLayer->addChild(title);
  auto info = CCLabelBMFont::create("From Zero / Startpos\nNoclip: Death / Accuracy\nSpeedhack: separate category\n\nReplay integrity: not verified", "goldFont.fnt");
  info->setAlignment(kCCTextAlignmentCenter); info->setScale(0.42f); info->setPosition({150.f,85.f}); m_mainLayer->addChild(info);
  return true;
 }
public:
 static ProgressPanel* create(GJGameLevel* level) {
  auto ret = new ProgressPanel();
  if (ret && ret->initForLevel(level)) { ret->autorelease(); return ret; }
  CC_SAFE_DELETE(ret); return nullptr;
 }
};
}
class $modify(GMPLevelInfoLayer, LevelInfoLayer) {
 bool init(GJGameLevel* level, bool challenge) {
  if (!LevelInfoLayer::init(level, challenge)) return false;
  auto menu = CCMenu::create(); if (!menu) return true;
  auto sprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
  if (!sprite) sprite = CCSprite::create("GJ_arrow_01_001.png");
  if (!sprite) { log::warn("GD Manager Progress: arrow sprite unavailable."); return true; }
  auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(GMPLevelInfoLayer::onOpenProgress));
  if (!button) return true;
  menu->addChild(button); menu->setPosition({0.f,0.f});
  button->setPosition({28.f, CCDirector::sharedDirector()->getWinSize().height - 55.f});
  menu->setID("gmp-info-menu"); this->addChild(menu,100);
  return true;
 }
 void onOpenProgress(CCObject*) {
  if (auto panel = ProgressPanel::create(m_level)) panel->show();
 }
};
