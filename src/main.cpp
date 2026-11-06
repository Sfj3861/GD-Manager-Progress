#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
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
        bool replayPlayback = false;
        bool active = false;
        std::uint64_t droppedEvents = 0;
        std::vector<InputEvent> events;
    };

    // Keep long sessions from growing the in-memory event buffer without bound.
    static constexpr std::size_t MAX_RECORDED_EVENTS = 250000;

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

    static int currentFrame(GJBaseGameLayer* layer) {
        if (!layer) return 0;
        // Geometry Dash's game-state progress advances by two units per frame.
        return std::max(0, static_cast<int>(layer->m_gameState.m_currentProgress) / 2);
    }

    static void begin(PlayLayer* layer, bool replayPlayback = false) {
        auto& r = recorder();
        r = Recorder{};
        if (!layer || !layer->m_level) return;

        r.levelID = layer->m_level->m_levelID.value();
        if (r.levelID <= 0) return;

        r.levelName = layer->m_level->m_levelName.c_str();
        r.startPercent = std::clamp(layer->getCurrentPercentInt(), 0, 100);
        r.practice = layer->m_isPracticeMode;
        r.testMode = layer->m_isTestMode;
        r.replayPlayback = replayPlayback;
        r.active = true;
        r.attempt = layer->m_attempts;
    }

    static void recordInput(GJBaseGameLayer* layer, bool down, int button, bool player2) {
        auto& r = recorder();
        if (!r.active || r.replayPlayback || !layer) return;
        auto* play = PlayLayer::get();
        if (!play || static_cast<GJBaseGameLayer*>(play) != layer) return;
        if (play->m_isPaused || play->m_hasCompletedLevel) return;
        if (r.events.size() >= MAX_RECORDED_EVENTS) {
            ++r.droppedEvents;
            return;
        }
        r.events.push_back({ currentFrame(layer), button, player2, down });
    }

    static bool writeReplay() {
        auto& r = recorder();
        if (!r.active || r.replayPlayback || r.levelID <= 0) return false;

        auto dir = Mod::get()->getSaveDir() / "replays";
        if (auto err = utils::file::createDirectoryAll(dir); err.isErr()) {
            log::error("Could not create replay directory: {}", err.unwrapErr());
            return false;
        }

        auto now = std::chrono::system_clock::now();
        auto stamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        auto stampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        auto* play = PlayLayer::get();
        auto endFrame = currentFrame(play);
        static std::uint64_t sequence = 0;
        auto filename = std::to_string(r.levelID) + "_" + std::to_string(stampMs)
            + "_" + std::to_string(++sequence) + ".gdr2";
        auto path = dir / filename;

        std::ostringstream out;
        out << "{\n"
            << "  \"format\": \"GDR2\",\n"
            << "  \"schemaVersion\": 1,\n"
            << "  \"formatNote\": \"GD Manager Progress event export; not asserted compatible with other replay tools\",\n"
            << "  \"levelId\": " << r.levelID << ",\n"
            << "  \"levelName\": \"" << jsonEscape(r.levelName) << "\",\n"
            << "  \"completion\": 100,\n"
            << "  \"completedAtUnixMs\": " << stampMs << ",\n"
            << "  \"endFrameEstimate\": " << endFrame << ",\n"
            << "  \"startPercent\": " << r.startPercent << ",\n"
            << "  \"attempt\": " << r.attempt << ",\n"
            << "  \"mode\": \"" << (r.practice ? "practice" : "normal") << "\",\n"
            << "  \"testMode\": " << (r.testMode ? "true" : "false") << ",\n"
            << "  \"integrity\": \"unverified\",\n"
            << "  \"integrityReason\": \"External tools and gameplay modifications are not comprehensively detectable\",\n"
            << "  \"inputEventLimit\": " << MAX_RECORDED_EVENTS << ",\n"
            << "  \"droppedInputEvents\": " << r.droppedEvents << ",\n"
            << "  \"inputCaptureTruncated\": " << (r.droppedEvents > 0 ? "true" : "false") << ",\n"
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
            if (history.peek() == std::ifstream::traits_type::eof()) {
                history << "level_id,completion,mode,integrity,timestamp_unix_s,input_events,dropped_events\n";
            }
            history << r.levelID << ",100," << (r.practice ? "practice" : "normal")
                    << ",unverified," << stamp << "," << r.events.size()
                    << "," << r.droppedEvents << "\n";
        } else {
            log::warn("Could not append completion index: {}", historyPath.string());
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
        gmp::begin(this, useReplay);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        // Preserve replay-playback state while resetting the captured attempt.
        bool replayPlayback = gmp::recorder().replayPlayback;
        gmp::begin(this, replayPlayback);
    }

    void levelComplete() {
        auto& r = gmp::recorder();
        if (r.active && !r.replayPlayback) {
            // Practice/editor completions are exportable, but always remain unverified.
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
            "Input capture: basic event recording\nRun categories: not auto-detected\n"
            "Startpos / Noclip / Speedhack: unverified\n\n"
            "Completed runs export as .gdr2\nReplay playback is excluded\n"
            "Integrity: always unverified",
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
