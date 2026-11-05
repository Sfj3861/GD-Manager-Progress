#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace gmp {
    struct InputEvent {
        int frame = 0;
        int button = 0;
        bool player2 = false;
        bool pressed = false;
    };

    struct Recorder {
        int levelID = 0;
        std::string levelName;
        int startPercent = 0;
        int attempt = 0;
        bool practice = false;
        bool testMode = false;
        bool active = false;
        std::vector<InputEvent> events;
    };

    static Recorder& recorder() {
        static Recorder value;
        return value;
    }

    static std::string jsonEscape(std::string const& value) {
        std::ostringstream out;
        for (unsigned char c : value) {
            switch (c) {
                case '\\': out << "\\\\"; break;
                case '"': out << "\\\""; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default:
                    if (c < 0x20) out << ' ';
                    else out << static_cast<char>(c);
            }
        }
        return out.str();
    }

    static std::string safeName(std::string value) {
        for (auto& c : value) {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'))
                c = '_';
        }
        if (value.empty()) value = "level";
        return value;
    }

    static int currentFrame(GJBaseGameLayer* layer) {
        if (!layer) return 0;
        // Geometry Dash's game-state progress advances by two units per frame.
        return std::max(0, static_cast<int>(layer->m_gameState.m_currentProgress) / 2);
    }

    static void begin(PlayLayer* layer) {
        auto& r = recorder();
        r = Recorder{};
        if (!layer || !layer->m_level) return;

        r.levelID = layer->m_level->m_levelID.value();
        if (r.levelID <= 0) return;

        r.levelName = layer->m_level->m_levelName.c_str();
        r.startPercent = std::clamp(layer->getCurrentPercentInt(), 0, 100);
        r.practice = layer->m_isPracticeMode;
        r.testMode = layer->m_isTestMode;
        r.active = true;
        r.attempt = layer->m_attempts;
    }

    static void recordInput(GJBaseGameLayer* layer, bool down, int button, bool player2) {
        auto& r = recorder();
        if (!r.active || !layer) return;
        auto* play = PlayLayer::get();
        if (!play || static_cast<GJBaseGameLayer*>(play) != layer) return;
        if (play->m_isPaused || play->m_hasCompletedLevel) return;
        r.events.push_back({ currentFrame(layer), button, player2, down });
    }

    static bool writeReplay() {
        auto& r = recorder();
        if (!r.active || r.levelID <= 0) return false;

        auto dir = Mod::get()->getSaveDir() / "replays";
        if (auto err = utils::file::createDirectoryAll(dir); err.isErr()) {
            log::error("Could not create replay directory: {}", err.unwrapErr());
            return false;
        }

        auto now = std::chrono::system_clock::now();
        auto stamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        auto filename = std::to_string(r.levelID) + "_" + std::to_string(stamp) + ".gdr2";
        auto path = dir / filename;

        std::ostringstream out;
        out << "{\n"
            << "  \"format\": \"GDR2\",\n"
            << "  \"schemaVersion\": 1,\n"
            << "  \"formatNote\": \"GD Manager Progress event export; not asserted compatible with other replay tools\",\n"
            << "  \"levelId\": " << r.levelID << ",\n"
            << "  \"levelName\": \"" << jsonEscape(r.levelName) << "\",\n"
            << "  \"completion\": 100,\n"
            << "  \"startPercent\": " << r.startPercent << ",\n"
            << "  \"attempt\": " << r.attempt << ",\n"
            << "  \"mode\": \"" << (r.practice ? "practice" : "normal") << "\",\n"
            << "  \"testMode\": " << (r.testMode ? "true" : "false") << ",\n"
            << "  \"integrity\": \"unverified\",\n"
            << "  \"integrityReason\": \"External tools and gameplay modifications are not comprehensively detectable\",\n"
            << "  \"inputs\": [";
        for (size_t i = 0; i < r.events.size(); ++i) {
            auto const& e = r.events[i];
            out << (i ? ",\n" : "\n")
                << "    {\"frame\":" << e.frame
                << ",\"button\":" << e.button
                << ",\"player2\":" << (e.player2 ? "true" : "false")
                << ",\"down\":" << (e.pressed ? "true" : "false") << "}";
        }
        if (!r.events.empty()) out << "\n  ";
        out << "]\n}\n";

        auto result = utils::file::writeStringSafe(path, out.str());
        if (result.isErr()) {
            log::error("Could not save GDR2 replay: {}", result.unwrapErr());
            return false;
        }

        auto historyPath = Mod::get()->getSaveDir() / "completions.csv";
        std::ofstream history(historyPath.string(), std::ios::app);
        if (history) {
            history << r.levelID << ",100," << (r.practice ? "practice" : "normal")
                    << ",unverified," << stamp << "," << r.events.size() << "\n";
        }
        log::info("Saved unverified GDR2 input export: {}", path.string());
        return true;
    }
}

class $modify(GMPInputCapture, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        gmp::recordInput(this, down, button, !isPlayer1);
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

class $modify(GMPPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        gmp::begin(this);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        // A reset starts a new attempt. Keep only the inputs from the attempt
        // that eventually completes the level.
        gmp::begin(this);
    }

    void levelComplete() {
        auto& r = gmp::recorder();
        if (r.active && !m_isPracticeMode && !m_isTestMode) {
            gmp::writeReplay();
        } else if (r.active) {
            // Practice/editor completions are still exportable, but never treated
            // as legitimate completions.
            gmp::writeReplay();
        }
        PlayLayer::levelComplete();
        r.active = false;
    }

    void onQuit() {
        gmp::recorder().active = false;
        PlayLayer::onQuit();
    }
};

namespace {
class ProgressPanel final : public FLAlertLayer {
protected:
    bool initForLevel(GJGameLevel* level) {
        if (!FLAlertLayer::init(nullptr, "GD Manager Progress", "", "Close", nullptr, 300.f, false, 205.f, 1.f))
            return false;

        auto title = CCLabelBMFont::create(
            level ? level->m_levelName.c_str() : "Selected level", "bigFont.fnt");
        title->setScale(0.45f);
        title->setPosition({150.f, 165.f});
        m_mainLayer->addChild(title);

        auto info = CCLabelBMFont::create(
            "Run categories\nFrom Zero / Startpos\nNoclip Death / Noclip Accuracy / Speedhack\n\n"
            "Completed replays are exported as .gdr2\nIntegrity: Unverified by default\n"
            "Unknown modifications are never marked legitimate",
            "goldFont.fnt");
        info->setAlignment(kCCTextAlignmentCenter);
        info->setScale(0.38f);
        info->setPosition({150.f, 95.f});
        m_mainLayer->addChild(info);

        auto hint = CCLabelBMFont::create(
            "Saved in this mod’s Geode save folder / replays",
            "chatFont.fnt");
        hint->setScale(0.34f);
        hint->setPosition({150.f, 27.f});
        m_mainLayer->addChild(hint);
        return true;
    }

public:
    static ProgressPanel* create(GJGameLevel* level) {
        auto ret = new ProgressPanel();
        if (ret && ret->initForLevel(level)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};
}

class $modify(GMPLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        auto menu = CCMenu::create();
        if (!menu) return true;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        if (!sprite) sprite = CCSprite::create("GJ_arrow_01_001.png");
        if (!sprite) {
            log::warn("GD Manager Progress: arrow sprite unavailable.");
            return true;
        }

        auto button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(GMPLevelInfoLayer::onOpenProgress));
        if (!button) return true;

        menu->addChild(button);
        menu->setPosition({0.f, 0.f});
        button->setPosition({28.f, CCDirector::sharedDirector()->getWinSize().height - 55.f});
        menu->setID("gmp-info-menu");
        this->addChild(menu, 100);
        return true;
    }

    void onOpenProgress(CCObject*) {
        if (auto panel = ProgressPanel::create(m_level)) panel->show();
    }
};
