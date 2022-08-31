//
// Copyright (c) Oneiro Games. All rights reserved.
// Licensed under the GNU General Public License, Version 3.0.
//

#include "TSOCAApp.hpp"
#include "Oneiro/Animation/DissolveAnimation.hpp"
#include "Oneiro/Core/Random.hpp"
#include "Oneiro/Lua/LuaCharacter.hpp"
#include "Oneiro/Lua/LuaTextBox.hpp"
#include "Oneiro/Renderer/Gui/GuiLayer.hpp"
#include "Oneiro/Renderer/ParticleSystem.hpp"
#include "Oneiro/Renderer/Renderer.hpp"
#include "Oneiro/Runtime/Engine.hpp"
#include "Oneiro/VisualNovel/VNCore.hpp"
#include "yaml-cpp/node/parse.h"
#include "yaml-cpp/yaml.h"
#include <string>

namespace oe::Renderer::GuiLayer
{
    inline void HelpMarker(const std::string& desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    inline void AlignForWidth(float width, float alignment)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        float avail = ImGui::GetContentRegionAvail().x;
        float off = (avail - width) * alignment;
        if (off > 0.0f)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
    }

    inline void AlignText(const std::initializer_list<std::string>& names, float widthOffset)
    {
        const auto& style = GetStyle();
        float width{};
        for (const auto& name : names)
        {
            width += GuiLayer::CalcTextSize(name.c_str()).x;
            width += style.ItemSpacing.x;
        }
        width += widthOffset;
        AlignForWidth(width);
    }

    inline void CreateSavesPopupModal(std::vector<std::filesystem::path>& saves, const std::string& id,
                                      const std::function<void(std::vector<std::filesystem::path>&)>& saveFilesFunc,
                                      const std::function<void(uint32_t& selected)>& okFunc, ImGuiWindowFlags flags)
    {
        const ImVec2 center = GuiLayer::GetMainViewport()->GetCenter();
        GuiLayer::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        static uint32_t selected{};
        if (GuiLayer::BeginPopupModal(id.c_str(), nullptr, flags))
        {
            auto localSaves = saves;
            if (saveFilesFunc)
                saveFilesFunc(localSaves);
            if (GuiLayer::BeginCombo("##localSaves", localSaves[selected].string().c_str()))
            {
                for (uint32_t i{}; i < localSaves.size(); ++i)
                {
                    const bool isSelected = (selected == i);
                    if (GuiLayer::Selectable(localSaves[i].string().c_str(), isSelected))
                        selected = i;

                    if (isSelected)
                        GuiLayer::SetItemDefaultFocus();
                }
                GuiLayer::EndCombo();
            }

            GuiLayer::Separator();

            if (GuiLayer::Button("Ок", ImVec2(120, 0)))
            {
                if (okFunc)
                    okFunc(selected);
                GuiLayer::CloseCurrentPopup();
            }
            GuiLayer::SetItemDefaultFocus();
            GuiLayer::SameLine();
            if (GuiLayer::Button("Отмена", ImVec2(120, 0)))
                GuiLayer::CloseCurrentPopup();
            GuiLayer::EndPopup();
        }
    };
} // namespace oe::Renderer::GuiLayer

namespace TSOCA
{
    bool Application::OnPreInit()
    {
        LoadGuiFont();
        SetupGuiStyle();

        if (!std::filesystem::exists("Saves/"))
            std::filesystem::create_directory("Saves");

        InitScripts();

        if (mConfigData.IsFileExists())
            mConfigData.Load();

        SetMonitorFromConfig();

        if (mConfigData.windowFullScreen)
            SetFullScreenFromConfig();

        mMainMenuMusic.LoadFromFile("Assets/Audio/Music/main_theme.ogg");

        Hazel::Audio::SetGlobalVolume(mConfigData.audioVolume);
        oe::VisualNovel::SetTextSpeed(mConfigData.textSpeed);

        LoadWindowIcon();

        SaveSpecifications();

        return true;
    }

    bool Application::OnInit()
    {
        mMainMenuMusic.Play();
        return true;
    }

    bool Application::OnUpdate(float deltaTime)
    {
        using namespace oe;

        UpdateSettingsMenu(deltaTime);

        if (mIsStart)
        {
            UpdateMainMenu(deltaTime);
        }
        else
        {
            if (!mShowEscapeMenu && mAutoNextStep)
            {
                mAutoSkipTotalTime += deltaTime;
                if (mAutoSkipTotalTime >= mConfigData.autoSkipTime)
                {
                    VisualNovel::NextStep();
                    mAutoSkipTotalTime = 0.0f;
                }
            }

            Core::Root::GetWorld()->UpdateEntities();
            VisualNovel::Update(deltaTime, !mShowEscapeMenu);

            UpdateSavesMenu(deltaTime);
            UpdateHistoryMenu(deltaTime);
            UpdateDebugInfo(deltaTime);
            UpdateEscapeMenu(deltaTime);

            if (mShowDemoWindow)
                Renderer::GuiLayer::ShowDemoWindow();

            ProcessVnWaiting(deltaTime);

            if (!mAutoNextStep)
            {
                if (mShowEscapeMenu)
                    Runtime::Engine::SetDeltaTimeMultiply(0.0f);
                else
                    Runtime::Engine::SetDeltaTimeMultiply(1.0f);
            }
        }

        Renderer::ResetStats();

        return true;
    }

    void Application::OnEvent(const oe::Core::Event::Base& e)
    {
        using namespace oe;
        using namespace oe::Core;
        if (typeid(Event::MouseButtonEvent) == typeid(e))
        {
            const auto& mouseButtonEvent = dynamic_cast<const Event::MouseButtonEvent&>(e);
            if (mouseButtonEvent.Button == Input::LEFT && mouseButtonEvent.Action == Input::PRESS && !mShowEscapeMenu)
                VisualNovel::NextStep();
            return;
        }

        if (typeid(Event::KeyInputEvent) == typeid(e))
        {
            const auto& keyInputEvent = dynamic_cast<const Event::KeyInputEvent&>(e);
            if (keyInputEvent.Action == Input::PRESS)
            {
                switch (keyInputEvent.Key)
                {
                case Input::D: mShowDebugInfoMenu = !mShowDebugInfoMenu; return;
                case Input::S: mShowSavesMenu = !mShowSavesMenu && !mIsStart; return;
                case Input::H: mShowHistoryMenu = !mShowHistoryMenu && !mIsStart; return;
                case Input::SPACE:
                    if (!mShowEscapeMenu)
                        VisualNovel::NextStep();
                    return;
                case Input::ENTER:
                    if (!mShowEscapeMenu)
                        VisualNovel::NextStep();
                    return;
                case Input::J: {
                    mAutoNextStep = !mAutoNextStep && !mIsStart && !mShowEscapeMenu;
                    Runtime::Engine::SetDeltaTimeMultiply(mAutoNextStep ? 5.0f : 1.0f);
                    return;
                }
                case Input::ESC: {
                    if (!mShowSettingsMenu && !mShowHistoryMenu && !mShowSavesMenu && !mIsStart && !mShowAcceptPopupModal)
                        mShowEscapeMenu = !mShowEscapeMenu;
                    else
                    {
                        mShowSavesMenu = false;
                        mShowSettingsMenu = false;
                        mShowHistoryMenu = false;
                        mShowAcceptPopupModal = false;
                    }
                    return;
                }
                default: return;
                }
            }
        }
    }

    void Application::OnShutdown()
    {
        using namespace oe;
        auto particleSystemEntity = Core::Root::GetWorld()->GetEntity("ParticleSystem");
        if (particleSystemEntity.HasComponent<ParticleSystemComponent>())
            particleSystemEntity.GetComponent<ParticleSystemComponent>().DestroyParticleProps("main");
        if (VisualNovel::GetCurrentLabel() == "start")
            VisualNovel::Shutdown();
        Core::Root::GetWorld()->DestroyEntity(particleSystemEntity);
        mConfigData.Save();
    }

    void Application::UpdateMainMenu(float deltaTime)
    {
        using namespace oe::Renderer;
        bool exit{};
        bool need2Start{};

        GuiLayer::SetNextWindowPos(GuiLayer::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        GuiLayer::SetNextWindowBgAlpha(0.5f);
        GuiLayer::Begin("MainMenu", nullptr,
                        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                            ImGuiWindowFlags_NoMove);

        if (GuiLayer::Button("Начать", ImVec2(100, 30)))
        {
            mMainMenuMusic.Stop();
            mMainMenuMusic.~Source();
            oe::VisualNovel::Init(&mScriptFile, false);
            oe::Core::Root::GetWorld()->GetEntity("ParticleSystem").AddComponent<oe::ParticleSystemComponent>();
            mIsStart = false;
        }
        if (oe::World::World::IsExists("Saves/auto_save"))
        {
            if (GuiLayer::Button("Продолжить", ImVec2(100, 30)))
            {
                mMainMenuMusic.Stop();
                mMainMenuMusic.~Source();
                oe::VisualNovel::Init(&mScriptFile);
                oe::Core::Root::GetWorld()->GetEntity("ParticleSystem").AddComponent<oe::ParticleSystemComponent>();
                mIsStart = false;
            }
        }
        else
        {
            auto& style = GuiLayer::GetStyle().Colors;
            GuiLayer::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.1f));
            GuiLayer::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.1f));
            GuiLayer::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.1f));
            GuiLayer::Button("Продолжить", ImVec2(100, 30));
            GuiLayer::PopStyleColor(3);
        }
        if (std::distance(std::filesystem::directory_iterator("Saves/"), std::filesystem::directory_iterator{}) > 0)
        {
            if (GuiLayer::Button("Сохранения", ImVec2(100, 30)))
                mShowSavesMenu = !mShowSavesMenu;
        }
        else
        {
            GuiLayer::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.1f));
            GuiLayer::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.1f));
            GuiLayer::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.1f));
            GuiLayer::Button("Сохранения", ImVec2(100, 30));
            GuiLayer::PopStyleColor(3);
        }
        if (GuiLayer::Button("Настройки", ImVec2(100, 30)))
            mShowSettingsMenu = !mShowSettingsMenu;
        if (GuiLayer::Button("Выйти", ImVec2(100, 30)))
            exit = true;

        GuiLayer::End();

        if (exit)
            Stop();

        if (mShowSavesMenu)
        {
            using namespace oe;
            using namespace Renderer;

            std::vector<std::filesystem::path> saves{};
            static std::string selectedSave{};
            const std::string fileName{"player_save"};
            const auto& path = std::filesystem::directory_iterator("Saves/");
            int it{};

            for (auto& file : path)
                saves.push_back(file.path());
            std::sort(saves.begin(), saves.end());
            for (auto& save : saves)
            {
                std::string saveFile = save.filename().replace_extension().string();
                if (saveFile.size() >= fileName.size())
                    saveFile.erase(fileName.size(), saveFile.size());
                if (saveFile == "player_save")
                    it++;
            }

            if (std::filesystem::exists(std::filesystem::path("Saves/player_save" + std::to_string(it) + ".oeworld")))
                it++;

            GuiLayer::Begin("Сохранения");
            if (GuiLayer::Button("Удалить сохранение") && !saves.empty())
                GuiLayer::OpenPopup("Удалить сохранение");

            GuiLayer::Separator();

            if (mShowAcceptPopupModal)
                GuiLayer::OpenPopup("Вы уверены?");

            if (GuiLayer::BeginListBox("Список сохранений", ImVec2(-FLT_MIN, GuiLayer::GetWindowHeight())))
            {
                for (uint32_t i{}; i < saves.size(); ++i)
                {
                    auto& save = saves[i];
                    if (!save.empty())
                    {
                        const bool isSelected = (mSelectedSave == i);
                        if (ImGui::Selectable(save.string().c_str(), isSelected))
                        {
                            mShowAcceptPopupModal = true;
                            selectedSave = save.replace_extension().string();
                            break;
                        }
                    }
                }
                GuiLayer::EndListBox();
            }

            RenderAcceptPopupModal(selectedSave, GuiLayer::GetMainViewport()->GetCenter());

            GuiLayer::CreateSavesPopupModal(saves, "Удалить сохранение", nullptr, [&](auto& selected) {
                std::filesystem::remove(saves[selected]);
            });

            GuiLayer::End();
        }
    }

    void Application::UpdateSavesMenu(float deltaTime)
    {
        if (mShowSavesMenu)
        {
            using namespace oe;
            using namespace Renderer;

            static std::string selectedSave{};

            std::vector<std::filesystem::path> saves{};
            const auto& path = std::filesystem::directory_iterator("Saves/");
            const std::string fileName{"player_save"};
            int it{};

            for (auto& file : path)
                saves.push_back(file.path());
            std::sort(saves.begin(), saves.end());

            for (auto& save : saves)
            {
                std::string saveFile = save.filename().replace_extension().string();
                if (saveFile.size() >= fileName.size())
                    saveFile.erase(fileName.size(), saveFile.size());
                if (saveFile == "player_save")
                    it++;
            }

            if (std::filesystem::exists(std::filesystem::path("Saves/player_save" + std::to_string(it) + ".oeworld")))
                it++;

            const auto& itStr = std::to_string(it);

            GuiLayer::Begin("Сохранения");
            if (GuiLayer::Button("Сохранить"))
            {
                const auto& currentLabel = VisualNovel::GetCurrentLabel();
                if (currentLabel == "start")
                    VisualNovel::Save("Saves/" + fileName + itStr, fileName + itStr);
                else
                    GuiLayer::OpenPopup("Упс...");
            }

            if (GuiLayer::Button("Перезаписать сохранение") && !saves.empty())
            {
                const auto& currentLabel = VisualNovel::GetCurrentLabel();
                if (currentLabel == "start")
                    GuiLayer::OpenPopup("Перезаписать сохранение");
                else
                    GuiLayer::OpenPopup("Упс...");
            }

            if (GuiLayer::Button("Удалить сохранение") && !saves.empty())
                GuiLayer::OpenPopup("Удалить сохранение");

            GuiLayer::Separator();

            if (mShowAcceptPopupModal)
                GuiLayer::OpenPopup("Вы уверены?");

            if (GuiLayer::BeginListBox("Список сохранений", ImVec2(-FLT_MIN, GuiLayer::GetWindowHeight())))
            {
                for (uint32_t i{}; i < saves.size(); ++i)
                {
                    auto& save = saves[i];
                    if (!save.empty())
                    {
                        const bool isSelected = (mSelectedSave == i);
                        if (ImGui::Selectable(save.string().c_str(), isSelected))
                        {
                            mShowAcceptPopupModal = true;
                            selectedSave = save.replace_extension().string();
                            break;
                        }
                    }
                }
                GuiLayer::EndListBox();
            }

            const ImVec2 center = GuiLayer::GetMainViewport()->GetCenter();
            GuiLayer::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (GuiLayer::BeginPopupModal("Упс...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize))
            {
                GuiLayer::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "В данный момент сохранения отключены!");

                GuiLayer::AlignText({"Ок"}, 55.0f);
                if (GuiLayer::Button("Ок", ImVec2(80, 25)))
                    GuiLayer::CloseCurrentPopup();
                GuiLayer::EndPopup();
            }

            RenderAcceptPopupModal(selectedSave, center);

            GuiLayer::CreateSavesPopupModal(saves, "Перезаписать сохранение", nullptr, [&](auto& selected) {
                const auto& currentLabel = VisualNovel::GetCurrentLabel();
                if (currentLabel == "start")
                {
                    oe::VisualNovel::Save(saves[selected].replace_extension().string(),
                                          saves[selected].filename().replace_extension().string());
                    selected = 0;
                    selectedSave.clear();
                }
                else
                    GuiLayer::OpenPopup("Упс...");
            });

            GuiLayer::CreateSavesPopupModal(saves, "Удалить сохранение", nullptr, [&](auto& selected) {
                std::filesystem::remove(saves[selected]);
                selected = 0;
                selectedSave.clear();
            });

            GuiLayer::End();
        }
        else
            mShowAcceptPopupModal = false;
    }

    void Application::RenderAcceptPopupModal(const std::string& selectedSave, const ImVec2& center)
    {
        using namespace oe;
        using namespace oe::Renderer;
        static const auto loadSave = [&]() {
            if (mIsStart)
            {
                mMainMenuMusic.Stop();
                mMainMenuMusic.~Source();
                VisualNovel::Init(&mScriptFile, false);
            }
            if (!VisualNovel::LoadSave(&mScriptFile, selectedSave))
                OE_LOG_WARNING("Failed to load world '" + selectedSave + "'!")
            mShowAcceptPopupModal = false;
            mShowSavesMenu = false;
            if (mIsStart)
            {
                Core::Root::GetWorld()->GetEntity("ParticleSystem").AddComponent<ParticleSystemComponent>();
                mIsStart = false;
            }
        };
        GuiLayer::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (GuiLayer::BeginPopupModal("Вы уверены?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (mConfigData.renderAcceptPopupModal)
            {
                GuiLayer::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s",
                                      ("Вы собираетесь загрузить сохранение \"" + selectedSave + ".oeworld" + "\"").c_str());
                GuiLayer::Separator();

                GuiLayer::AlignText({"Ок", "Отмена"}, 145);

                if (GuiLayer::Button("Ок", ImVec2(120, 25)))
                {
                    loadSave();
                    ImGui::CloseCurrentPopup();
                }

                GuiLayer::SetItemDefaultFocus();
                GuiLayer::SameLine();

                if (GuiLayer::Button("Отмена", ImVec2(120, 25)))
                {
                    GuiLayer::CloseCurrentPopup();
                    mShowAcceptPopupModal = false;
                }
            }
            else
            {
                loadSave();
                GuiLayer::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void Application::UpdateHistoryMenu(float deltaTime)
    {
        const auto& window = oe::Core::Root::GetWindow();
        if (mShowHistoryMenu)
        {
            using namespace oe::Renderer;

            GuiLayer::Begin("История");

            if (GuiLayer::BeginListBox("Список истории", ImVec2(-FLT_MIN, GuiLayer::GetWindowHeight() / 1.25f)))
            {
                const auto currentIt = oe::VisualNovel::GetCurrentIterator();
                static auto prevIt = currentIt;
                const auto& instructions = oe::VisualNovel::GetInstructions();
                for (uint32_t i{}; i < currentIt; ++i)
                {
                    const auto& instruction = instructions[i];
                    if (instructions[i].EqualType(oe::VisualNovel::SAY_TEXT))
                    {
                        const std::string text =
                            instruction.characterData.character->GetName().empty()
                                ? instruction.characterData.text
                                : instruction.characterData.character->GetName() + ": " + instruction.characterData.text;
                        std::string text2u8{};
                        bool checkForParseCmd{};
                        bool parseCmd{};

                        const auto parseCommand = [&](char ch) {
                            if (checkForParseCmd)
                            {
                                if (ch == '/')
                                {
                                    checkForParseCmd = false;
                                    parseCmd = true;
                                    return false;
                                }
                                else
                                {
                                    text2u8 += '[';
                                    checkForParseCmd = false;
                                }
                            }

                            if (parseCmd)
                            {
                                if (ch == ']')
                                {
                                    parseCmd = false;
                                    return false;
                                }
                                return false;
                            }

                            if (ch == '[')
                            {
                                checkForParseCmd = true;
                                return false;
                            }

                            return true;
                        };

                        for (auto& character : text)
                        {
                            if (!parseCommand(character))
                                continue;
                            text2u8 += character;
                        }

                        std::u8string u8String{text2u8.begin(), text2u8.end()};
                        GuiLayer::TextWrapped("%s", u8String.c_str());
                        GuiLayer::Separator();
                    }
                }
                if (mConfigData.autoScrollHistory && prevIt != currentIt)
                {
                    GuiLayer::SetScrollY(GuiLayer::GetWindowHeight() * GuiLayer::GetWindowHeight());
                    prevIt = currentIt;
                }
                GuiLayer::EndListBox();
            }
            GuiLayer::End();
        }
    }

    void Application::UpdateDebugInfo(float deltaTime)
    {
        if (mShowDebugInfoMenu)
        {
            using namespace oe;
            using namespace Renderer;

            double windowCursorPosX{}, windowCursorPosY{};
            glfwGetCursorPos(Core::Root::GetWindow()->GetGLFW(), &windowCursorPosX, &windowCursorPosY);
            const double glCursorPosX = (windowCursorPosX / Core::Root::GetWindow()->GetWidth() - 1.0f);
            const double glCursorPosY = -(windowCursorPosY / Core::Root::GetWindow()->GetHeight());

            auto& io = GuiLayer::GetIO();
            const auto& currentLabel = VisualNovel::GetCurrentLabel();
            const auto& prevBackground = VisualNovel::GetPrevBackground();
            const auto& currentBackground = VisualNovel::GetCurrentBackground();
            const auto& instructions = VisualNovel::GetInstructions();
            const auto& currentCharacters = VisualNovel::GetCurrentCharacters();
            const auto& renderingStats = Renderer::GetStats();
            auto currentIt = VisualNovel::GetCurrentIterator();

            GuiLayer::Begin("Debug Info");

            GuiLayer::Text("Frame Time: %.2fms", (deltaTime * 1000));
            GuiLayer::Text("Frames: %.2f", io.Framerate);

            GuiLayer::Text("Vertices per frame: %i", renderingStats.Vertices);
            GuiLayer::Text("Indices per frame: %i", renderingStats.Indices);
            GuiLayer::Text("Flushes per frame: %i", renderingStats.FlushesCount);

            GuiLayer::Text("Window cursor pos: %.f / %.f", windowCursorPosX, windowCursorPosY);
            GuiLayer::Text("GL cursor pos: %.2f / %.2f", glCursorPosX, glCursorPosY);

            GuiLayer::Text("Current label: %s", currentLabel.c_str());
            GuiLayer::Text("Current iterator: %i", currentIt);

            GuiLayer::Text("Is render choice menu: %i", VisualNovel::IsRenderChoiceMenu());

            if (GuiLayer::Button("Show Demo Window"))
                mShowDemoWindow = !mShowDemoWindow;

            GuiLayer::DragFloat("Auto Skip Time", &mConfigData.autoSkipTime, 0.01f, 0.0f, 5.0f);

            if (GuiLayer::CollapsingHeader("Instructions"))
            {
                if (GuiLayer::BeginListBox("Instructions List", ImVec2(-FLT_MIN, GuiLayer::GetWindowHeight() / 2)))
                {
                    for (uint32_t i{}; i < instructions.size(); ++i)
                    {
                        const bool isSelected = (currentIt == i + 1);
                        std::string title = std::to_string(i) + ": ";
                        const auto& instruction = instructions[i];
                        switch (instruction.type)
                        {
                        case VisualNovel::CHANGE_SCENE:
                            title += "Change Scene | " + instruction.sceneEntity.GetComponent<TagComponent>().Tag;
                            break;
                        case VisualNovel::SHOW_CHARACTER:
                            title += "Show Character | " + instruction.characterData.character->GetName() + ":" +
                                     instruction.characterData.emotion;
                            break;
                        case VisualNovel::HIDE_CHARACTER:
                            title += "Hide Character | " + instruction.characterData.character->GetName() + ":" +
                                     instruction.characterData.emotion;
                            break;
                        case VisualNovel::PLAY_MUSIC:
                            title += "Play Music | Is playing: " + std::to_string(instruction.audioSource->IsPlaying());
                            break;
                        case VisualNovel::STOP_MUSIC:
                            title += "Stop Music | Is playing: " + std::to_string(instruction.audioSource->IsPlaying());
                            break;
                        case VisualNovel::PLAY_SOUND:
                            title += "Play Sound | Is playing: " + std::to_string(instruction.audioSource->IsPlaying());
                            break;
                        case VisualNovel::STOP_SOUND:
                            title += "Stop Sound | Is playing: " + std::to_string(instruction.audioSource->IsPlaying());
                            break;
                        case VisualNovel::PLAY_AMBIENT:
                            title += "Play Ambient | Is playing: " + std::to_string(instruction.audioSource->IsPlaying());
                            break;
                        case VisualNovel::STOP_AMBIENT:
                            title += "Stop Ambient | Is playing: " + std::to_string(instruction.audioSource->IsPlaying());
                            break;
                        case VisualNovel::JUMP_TO_LABEL: title += "Jump To Label | " + instruction.label.name; break;
                        case VisualNovel::MOVE_CHARACTER: {
                            const auto& pos = instruction.characterData.character->GetCurrentPosition();
                            title += "Move Sprite | " + instruction.characterData.character->GetName() + ":" +
                                     instruction.characterData.emotion + " to (" + std::to_string(pos.x) + ", " + std::to_string(pos.y) +
                                     ", " + std::to_string(pos.z) + ")";
                            break;
                        }
                        case VisualNovel::SAY_TEXT:
                            title += "Say Text | " + instruction.characterData.character->GetName() + ": " + instruction.characterData.text;
                            break;
                        case VisualNovel::CHOICE_MENU: {
                            title += "Choice Menu | ";
                            int iter{};
                            for (const auto& item : instruction.choiceMenuItems)
                            {
                                if ((iter % 2) == 0)
                                    title += "var = " + item + "; ";
                                else
                                    title += "target = " + item + "; ";
                                iter++;
                            }
                            break;
                        }
                        case VisualNovel::SET_TEXT_SPEED: break;
                        case VisualNovel::SHOW_TEXTBOX: title += "Show Text Box | " + std::to_string(instruction.animationSpeed); break;
                        case VisualNovel::HIDE_TEXTBOX: title += "Hide Text Box | " + std::to_string(instruction.animationSpeed); break;
                        case VisualNovel::WAIT:
                            title += "Wait | " + std::to_string(instruction.animationSpeed) + " / " + std::string(instruction.target);
                            break;
                        case VisualNovel::CHANGE_TEXTBOX:
                            title += "Change Text Box | " + instruction.textBox->GetSprite()->GetTexture()->GetData()->Path;
                            break;
                        case VisualNovel::LOAD_FRAMEBUFFER_SHADER: title += "Load FrameBuffer Shader | " + instruction.target; break;
                        }

                        ImGui::Selectable(title.c_str(), isSelected);

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndListBox();
                }
            }

            if (GuiLayer::CollapsingHeader("Backgrounds"))
            {
                PushBackgroundInfo("Previous background", prevBackground);
                PushBackgroundInfo("Current background", currentBackground);
            }

            if (GuiLayer::CollapsingHeader("Current Characters"))
            {
                const auto currentCharactersCount = currentCharacters.size();
                for (uint32_t i{}; i < currentCharactersCount; ++i)
                {
                    const auto& character = currentCharacters[i];
                    if (GuiLayer::TreeNode(("Character" + std::to_string(i)).c_str()))
                    {
                        if (GuiLayer::TreeNode("Sprite2DComponent"))
                        {
                            const auto& sprite = character.GetComponent<Sprite2DComponent>().Sprite2D;
                            const auto& textureData = sprite->GetTexture()->GetData();
                            GuiLayer::Text("Alpha: %.2f", sprite->GetAlpha());
                            GuiLayer::Text("IsKeepAr: %i", sprite->IsKeepAR());
                            GuiLayer::Text("Texture path: %s", textureData->Path.c_str());
                            GuiLayer::Text("Texture width: %i", textureData->Width);
                            GuiLayer::Text("Texture height: %i", textureData->Height);
                            GuiLayer::Text("Texture channels: %i", textureData->Channels);
                            GuiLayer::TreePop();
                        }

                        if (GuiLayer::TreeNode("AnimationComponent"))
                        {
                            const auto& animation =
                                (Animation::DissolveAnimation<Renderer::GL::Sprite2D>*)character.GetComponent<AnimationComponent>()
                                    .Animation;
                            GuiLayer::Text("Is ended: %i", animation->IsEnded());
                            GuiLayer::Text("Is reversed: %i", animation->IsReversed());
                            GuiLayer::Text("Total time: %.2f", animation->GetTotalTime());
                            GuiLayer::Text("Speed: %.2f", animation->GetSpeed());
                            GuiLayer::TreePop();
                        }
                        GuiLayer::TreePop();
                    }
                }
            }

            GuiLayer::End();
        }
    }

    void Application::UpdateSettingsMenu(float deltaTime)
    {
        if (mShowSettingsMenu)
        {
            using namespace oe;
            using namespace Renderer;
            GuiLayer::Begin("Настройки", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem("Основное"))
                {
                    GuiLayer::Text("Задержка перемотки");
                    ImGui::SameLine();
                    GuiLayer::HelpMarker("По умолчанию: 0.05");
                    GuiLayer::DragFloat("##autoSkipTime", &mConfigData.autoSkipTime, 0.005f, 0.0f, 5.0f);

                    GuiLayer::Separator();

                    GuiLayer::Text("Авто-прокрутка истории");
                    ImGui::SameLine();
                    GuiLayer::HelpMarker("После перехода к следующей реплике прокручивает историю вниз.");
                    GuiLayer::Checkbox("##autoScrollHistory", &mConfigData.autoScrollHistory);

                    GuiLayer::Separator();

                    GuiLayer::Text("Предупреждение при загрузке сохранений");
                    ImGui::SameLine();
                    GuiLayer::HelpMarker(
                        "При каждой загрузке сохранения будет появляться окно с подтверждением/отменой загрузки выбранного сохранения.");
                    GuiLayer::Checkbox("##renderAcceptPopupModal", &mConfigData.renderAcceptPopupModal);

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Аудио"))
                {
                    GuiLayer::Text("Общая громкость аудио");
                    ImGui::SameLine();
                    GuiLayer::HelpMarker("По умолчанию: 0.45");
                    if (GuiLayer::DragFloat("##audioVolume", &mConfigData.audioVolume, 0.005f, 0.0f, 1.0f))
                        Hazel::Audio::SetGlobalVolume(mConfigData.audioVolume);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Текст"))
                {
                    GuiLayer::Text("Скорость текста");
                    ImGui::SameLine();
                    GuiLayer::HelpMarker("Влияет на задержку появления следующего символа в тексте.\nПри 100 текст появляется "
                                         "моментально.\nПо умолчанию: 80");
                    if (GuiLayer::DragFloat("##textSpeed", &mConfigData.textSpeed, 0.1f, 0.0f, 100.0f))
                        VisualNovel::SetTextSpeed(mConfigData.textSpeed);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Окно"))
                {
                    GuiLayer::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Настройки применяются только после перезахода!");

                    GuiLayer::Separator();

                    GuiLayer::Checkbox("Полный Экран", &mConfigData.windowFullScreen);

                    GuiLayer::Separator();

                    static uint32_t selected{};
                    int monitorsCount{};
                    const auto& monitors = glfwGetMonitors(&monitorsCount);
                    GuiLayer::Text("Основной монитор");
                    ImGui::SameLine();
                    GuiLayer::HelpMarker("При запуске игры перемещает окно на выбранный монитор.\nПо умолчанию выбран монитор из настроек "
                                         "операционной системы.");
                    if (GuiLayer::BeginCombo("##monitors", glfwGetMonitorName(monitors[selected])))
                    {
                        for (uint32_t i{}; i < monitorsCount; ++i)
                        {
                            const bool isSelected = (selected == i);
                            if (GuiLayer::Selectable(glfwGetMonitorName(monitors[selected]), isSelected))
                            {
                                selected = i;
                                mConfigData.windowMonitor = monitors[selected];
                                mConfigData.windowMonitorName = glfwGetMonitorName(monitors[selected]);
                            }

                            if (isSelected)
                                GuiLayer::SetItemDefaultFocus();
                        }
                        GuiLayer::EndCombo();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            GuiLayer::End();
        }
    }

    void Application::UpdateEscapeMenu(float deltaTime)
    {
        if (mShowEscapeMenu)
        {
            using namespace oe::Renderer;
            bool exit{};
            GuiLayer::SetNextWindowPos(GuiLayer::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            GuiLayer::SetNextWindowBgAlpha(0.5f);
            GuiLayer::Begin("EscapeMenu", nullptr,
                            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

            if (GuiLayer::Button("Продолжить", ImVec2(100, 30)))
                mShowEscapeMenu = false;
            if (GuiLayer::Button("Сохранения", ImVec2(100, 30)))
                mShowSavesMenu = true;
            if (GuiLayer::Button("Настройки", ImVec2(100, 30)))
                mShowSettingsMenu = true;
            if (GuiLayer::Button("История", ImVec2(100, 30)))
                mShowHistoryMenu = true;
            if (GuiLayer::Button("Выйти", ImVec2(100, 30)))
                exit = true;

            GuiLayer::End();

            if (exit)
                Stop();

            if (mShowSettingsMenu || mShowSavesMenu || mShowHistoryMenu || mIsStart)
                mShowEscapeMenu = false;
        }
    }

    void Application::ProcessVnWaiting(float deltaTime)
    {
        if (!mShowEscapeMenu && oe::VisualNovel::IsWaiting() && oe::VisualNovel::GetWaitTarget() == "end")
        {
            using namespace oe;
            using namespace Renderer;
            auto pProps =
                Core::Root::GetWorld()->GetEntity("ParticleSystem").GetComponent<oe::ParticleSystemComponent>().GetParticleProps("main");
            if (!pProps)
            {
                pProps = Core::Root::GetWorld()
                             ->GetEntity("ParticleSystem")
                             .GetComponent<ParticleSystemComponent>()
                             .CreateParticleProps("main", 10);
            }
            pProps->SizeBegin = 0.0075f;
            pProps->SizeEnd = 0.0025f;
            pProps->SizeVariation = 0.015f;
            pProps->ColorBegin = {1.0f, 0.0f, 0.5f, 1.0f};
            pProps->ColorEnd = {0.0f, 0.5f, 0.0f, 0.0f};
            pProps->VelocityVariation = {0.1f, 0.1f};
            pProps->LifeTime = 1.5f;
            pProps->RotationAngleBegin = 0.0f;
            pProps->RotationAngleEnd = 360.0f;
            pProps->Position = mParticlePositions[mCurrentParticlePos];
            if (mCurrentParticlePos == mParticlePositions.size())
                mCurrentParticlePos = 0;
            if (mCurrentParticlePos == 0)
                pProps->LifeTime = 0.0f;
            mCurrentParticlePos++;
        }
        else
            oe::Core::Root::GetWorld()
                ->GetEntity("ParticleSystem")
                .GetComponent<oe::ParticleSystemComponent>()
                .DestroyParticleProps("main");
    }

    void Application::PushBackgroundInfo(const std::string& title, const oe::World::Entity& background)
    {
        using namespace oe;
        using namespace Renderer;
        if (GuiLayer::TreeNode(title.c_str()))
        {
            if (GuiLayer::TreeNode("Sprite2DComponent/QuadComponent"))
            {
                if (background.HasComponent<Sprite2DComponent>())
                {
                    const auto& sprite = background.GetComponent<Sprite2DComponent>().Sprite2D;
                    const auto& textureData = sprite->GetTexture()->GetData();
                    GuiLayer::Text("Alpha: %.2f", sprite->GetAlpha());
                    GuiLayer::Text("IsKeepAr: %i", sprite->IsKeepAR());

                    GuiLayer::Text("Texture path: %s", textureData->Path.c_str());
                    GuiLayer::Text("Texture width: %i", textureData->Width);
                    GuiLayer::Text("Texture height: %i", textureData->Height);
                    GuiLayer::Text("Texture channels: %i", textureData->Channels);
                }
                else if (background.HasComponent<QuadComponent>())
                {
                    const auto& quad = background.GetComponent<QuadComponent>();
                    GuiLayer::Text("%.2f:%.2f:%.2f:%.2f", quad.Color.x, quad.Color.y, quad.Color.z, quad.Color.w);
                }
                GuiLayer::TreePop();
            }

            if (GuiLayer::TreeNode("AnimationComponent"))
            {
                if (background.HasComponent<Sprite2DComponent>())
                {
                    const auto& animation =
                        (Animation::DissolveAnimation<Renderer::GL::Sprite2D>*)background.GetComponent<AnimationComponent>().Animation;
                    GuiLayer::Text("Is ended: %i", animation->IsEnded());
                    GuiLayer::Text("Is reversed: %i", animation->IsReversed());
                    GuiLayer::Text("Total time: %.2f", animation->GetTotalTime());
                    GuiLayer::Text("Speed: %.2f", animation->GetSpeed());
                }
                else if (background.HasComponent<QuadComponent>())
                {
                    const auto& animation =
                        (Animation::DissolveAnimation<QuadComponent>*)background.GetComponent<AnimationComponent>().Animation;
                    GuiLayer::Text("Is ended: %i", animation->IsEnded());
                    GuiLayer::Text("Is reversed: %i", animation->IsReversed());
                    GuiLayer::Text("Total time: %.2f", animation->GetTotalTime());
                    GuiLayer::Text("Speed: %.2f", animation->GetSpeed());
                }
                GuiLayer::TreePop();
            }
            GuiLayer::TreePop();
        }
    }

    void Application::LoadGuiFont()
    {
        auto& io = ImGui::GetIO();
        ImFontConfig fontConfig;
        fontConfig.OversampleH = 3;
        fontConfig.OversampleV = 3;
        static constexpr ImWchar ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
            0x2DE0, 0x2DFF, // Cyrillic Extended-A
            0xA640, 0xA69F, // Cyrillic Extended-B
            0x2000, 0x206F, // Punctuation
            0,
        };
        io.Fonts->AddFontFromFileTTF("Assets/Fonts/gui.ttf", 15.5f, &fontConfig, ranges);
    }

    void Application::SetupGuiStyle()
    {
        auto& style = ImGui::GetStyle();
        style.ScrollbarSize = 12;
        style.ScrollbarRounding = 12;
        style.WindowTitleAlign.x = 0.5f;
    }

    void Application::InitScripts()
    {
        mScriptFile.OpenLibraries(sol::lib::base);
        mScriptFile.Init();
        mScriptFile.RequireFile("", "Assets/Scripts/resources.lua");
        mScriptFile.LoadFile("Assets/Scripts/config.lua", false);
        mScriptFile.LoadFile("Assets/Scripts/utils.lua", false);
        mScriptFile.LoadFile("Assets/Scripts/main.lua", false);
    }

    void Application::SetFullScreenFromConfig()
    {
        const auto* videoMode = glfwGetVideoMode(mConfigData.windowMonitor);
        glfwSetWindowMonitor(oe::Core::Root::GetWindow()->GetGLFW(), mConfigData.windowMonitor, 0, 0, videoMode->width, videoMode->height,
                             videoMode->refreshRate);
    }

    void Application::SetMonitorFromConfig()
    {
        if (mConfigData.windowMonitorName.empty())
        {
            auto* primaryMonitor = glfwGetPrimaryMonitor();
            mConfigData.windowMonitorName = std::string(glfwGetMonitorName(primaryMonitor));
        }

        int monitorsCount{};
        const auto& monitors = glfwGetMonitors(&monitorsCount);
        for (int i{}; i < monitorsCount; ++i)
        {
            auto* monitor = monitors[i];
            if (glfwGetMonitorName(monitor) == mConfigData.windowMonitorName)
                mConfigData.windowMonitor = monitor;
        }
    }

    void Application::LoadWindowIcon()
    {
        stbi_set_flip_vertically_on_load(0);
        GLFWimage windowIconImages[1];
        windowIconImages[0].pixels =
            stbi_load("Assets/Images/Misc/icon.png", &windowIconImages[0].width, &windowIconImages[0].height, nullptr, 4);

        if (windowIconImages[0].pixels)
            glfwSetWindowIcon(oe::Core::Root::GetWindow()->GetGLFW(), 1, windowIconImages);
        else
            OE_LOG_WARNING("Failed to load icon from 'Assets/Images/Misc/icon.png' path!");
        stbi_image_free(windowIconImages[0].pixels);
        stbi_set_flip_vertically_on_load(1);
    }

    void Application::SaveSpecifications()
    {
        std::ofstream file{"specifications.yaml"};
        if (!file.is_open())
            OE_LOG_WARNING("Failed to open specifications file!");

        YAML::Emitter out{};

        int extensionsCount{};
        int glMajorVersion{};
        int glMinorVersion{};
        int monitorsCount{};
        gl::GetIntegerv(gl::NUM_EXTENSIONS, &extensionsCount);
        gl::GetIntegerv(gl::MAJOR_VERSION, &glMajorVersion);
        gl::GetIntegerv(gl::MINOR_VERSION, &glMinorVersion);
        const auto& monitors = glfwGetMonitors(&monitorsCount);

        out << YAML::BeginMap; // Begin Specs
        out << YAML::Key << "Specs" << oe::Core::Random::DiceUuid();

        out << YAML::Key << "Basic";
        out << YAML::BeginMap; // Begin Basic
        out << YAML::Key << "OS";
        std::string osStr{};
        switch (glfwGetPlatform())
        {
        case GLFW_PLATFORM_WIN32: osStr += "Windows"; break;
        case GLFW_PLATFORM_X11: osStr += "GNU/Linux X11"; break;
        case GLFW_PLATFORM_WAYLAND: osStr += "GNU/Linux Wayland"; break;
        }

        osStr += ' ';
#if defined(__x86_64__) || defined(_WIN64)
        osStr += "x64";
#else
        osStr += "x32";
#endif
        out << osStr;

        out << YAML::EndMap; // End Basic

        out << YAML::Key << "Monitors";
        out << YAML::BeginMap; // Begin Monitors
        for (uint32_t i{}; i < monitorsCount; ++i)
        {
            const auto& monitor = monitors[i];
            const auto& videoMode = glfwGetVideoMode(monitor);
            std::pair<int, int> monitorSize{};
            std::pair<float, float> monitorContentScale{};
            glfwGetMonitorPhysicalSize(monitor, &monitorSize.first, &monitorSize.second);
            glfwGetMonitorContentScale(monitor, &monitorContentScale.first, &monitorContentScale.second);
            out << YAML::Key << std::to_string(i);
            out << YAML::BeginMap; // Begin Monitor

            out << YAML::Key << "Name" << glfwGetMonitorName(monitor);

            out << YAML::Key << "PhysicalSize";
            out << YAML::BeginMap; // Begin PhysicalSize
            out << YAML::Key << "Width" << monitorSize.first;
            out << YAML::Key << "Height" << monitorSize.second;
            out << YAML::EndMap; // End PhysicalSize

            out << YAML::Key << "ContentScale";
            out << YAML::BeginMap; // Begin ContentScale
            out << YAML::Key << "X" << monitorContentScale.first;
            out << YAML::Key << "Y" << monitorContentScale.second;
            out << YAML::EndMap; // End ContentScale

            out << YAML::Key << "VideoMode";
            out << YAML::BeginMap; // Begin VideoMode
            out << YAML::Key << "Width" << videoMode->width;
            out << YAML::Key << "Height" << videoMode->height;
            out << YAML::Key << "RefreshRate" << videoMode->refreshRate;
            out << YAML::Key << "Bits" << YAML::Flow;
            out << YAML::BeginSeq << videoMode->redBits << videoMode->greenBits << videoMode->blueBits << YAML::EndSeq;
            out << YAML::EndMap; // End VideoMode

            out << YAML::EndMap; // End Monitor
        }
        out << YAML::EndMap; // End Monitors

        out << YAML::Key << "OpenGL";
        out << YAML::BeginMap; // Begin OpenGL

        out << YAML::Key << "Version";
        out << YAML::BeginMap; // Begin Version
        out << YAML::Key << "Major" << std::to_string(glMajorVersion);
        out << YAML::Key << "Minor" << std::to_string(glMinorVersion);
        out << YAML::EndMap; // End Version

        out << YAML::Key << "Vendor" << (char*)gl::GetString(gl::VENDOR);
        out << YAML::Key << "Renderer" << (char*)gl::GetString(gl::RENDERER);
        out << YAML::Key << "ShadingLanguageVersion" << (char*)gl::GetString(gl::SHADING_LANGUAGE_VERSION);

        out << YAML::Key << "Extensions";
        out << YAML::BeginMap; // Begin Extensions
        for (uint32_t i{}; i < extensionsCount; ++i)
            out << YAML::Key << std::to_string(i) << (char*)gl::GetStringi(gl::EXTENSIONS, i);
        out << YAML::EndMap; // End Extensions

        out << YAML::EndMap; // End OpenGL
        out << YAML::EndMap; // End Specs

        file << out.c_str();
        file.close();
    }

    bool Application::ConfigData::IsFileExists() const
    {
        return std::filesystem::exists(std::filesystem::path(fileName));
    }

    void Application::ConfigData::Load()
    {
        const auto& cfgFile = YAML::LoadFile(fileName);
        const auto& basic = cfgFile["Basic"];
        const auto& audio = cfgFile["Audio"];
        const auto& window = cfgFile["Window"];
        const auto& text = cfgFile["Text"];

        if (basic)
        {
            const auto& autoSkipTimeCfg = basic["AutoSkipTime"];
            const auto& autoScrollHistoryCfg = basic["AutoScrollHistory"];
            const auto& renderAcceptPopupModalCfg = basic["RenderAcceptPopupModal"];
            if (autoSkipTimeCfg)
                autoSkipTime = autoSkipTimeCfg.as<float>();
            if (autoScrollHistoryCfg)
                autoScrollHistory = autoScrollHistoryCfg.as<bool>();
            if (renderAcceptPopupModalCfg)
                renderAcceptPopupModal = renderAcceptPopupModalCfg.as<bool>();
        }

        if (audio)
        {
            const auto& audioVolumeCfg = audio["Volume"];
            if (audioVolumeCfg)
                audioVolume = audioVolumeCfg.as<float>();
        }

        if (window)
        {
            const auto& windowMonitorCfg = window["Monitor"];
            const auto& windowFullScreenCfg = window["FullScreen"];
            if (windowMonitorCfg)
                windowMonitorName = windowMonitorCfg.as<std::string>();
            if (windowFullScreenCfg)
                windowFullScreen = windowFullScreenCfg.as<bool>();
        }

        if (text)
        {
            const auto& textSpeedCfg = text["Speed"];
            if (textSpeedCfg)
                textSpeed = textSpeedCfg.as<float>();
        }
    }

    void Application::ConfigData::Save()
    {
        std::ofstream cfgFile{fileName};
        if (!cfgFile.is_open())
            OE_LOG_WARNING("Failed to open config file!");

        YAML::Emitter out{};

        out << YAML::BeginMap; // Begin Config
        out << YAML::Key << "Config" << oe::Core::Random::DiceUuid();

        out << YAML::Key << "Basic";
        out << YAML::BeginMap; // Begin Basic
        out << YAML::Key << "AutoSkipTime" << YAML::Value << autoSkipTime;
        out << YAML::Key << "AutoScrollHistory" << YAML::Value << autoScrollHistory;
        out << YAML::Key << "RenderAcceptPopupModal" << YAML::Value << renderAcceptPopupModal;
        out << YAML::EndMap; // End Basic

        out << YAML::Key << "Audio";
        out << YAML::BeginMap; // Begin Audio
        out << YAML::Key << "Volume" << YAML::Value << audioVolume;
        out << YAML::EndMap; // End Audio

        out << YAML::Key << "Window";
        out << YAML::BeginMap; // Begin Window
        out << YAML::Key << "Monitor" << YAML::Value << windowMonitorName;
        out << YAML::Key << "FullScreen" << YAML::Value << windowFullScreen;
        out << YAML::EndMap; // End Window

        out << YAML::Key << "Text";
        out << YAML::BeginMap; // Begin Text
        out << YAML::Key << "Speed" << YAML::Value << textSpeed;
        out << YAML::EndMap; // End Text

        out << YAML::EndMap; // End Config

        cfgFile << out.c_str();
        cfgFile.close();
    }
} // namespace TSOCA

#pragma clang diagnostic push
#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"
namespace oe::Runtime
{
    std::shared_ptr<Application> CreateApplication(int, char*[])
    {
        return std::make_shared<TSOCA::Application>("The Suffering of Carl Allen", 1600, 900);
    }
} // namespace oe::Runtime
#pragma clang diagnostic pop