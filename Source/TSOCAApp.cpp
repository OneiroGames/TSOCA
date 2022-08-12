//
// Copyright (c) Oneiro Games. All rights reserved.
// Licensed under the GNU General Public License, Version 3.0.
//

#include "TSOCAApp.hpp"
#include "Oneiro/Animation/DissolveAnimation.hpp"
#include "Oneiro/Lua/LuaCharacter.hpp"
#include "Oneiro/Renderer/Gui/GuiLayer.hpp"
#include "Oneiro/Renderer/Renderer.hpp"
#include "Oneiro/Runtime/Engine.hpp"
#include "Oneiro/VisualNovel/VNCore.hpp"
#include <filesystem>

namespace TSOCA
{
    bool Application::OnInit()
    {
        using namespace oe;
        using namespace Renderer;

        auto& io = GuiLayer::GetIO();
        ImFontConfig fontConfig;
        fontConfig.OversampleH = 3;
        static const ImWchar ranges[] = {
            0x0020, 0x00FF, // Basic Latin + Latin Supplement
            0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
            0x2DE0, 0x2DFF, // Cyrillic Extended-A
            0xA640, 0xA69F, // Cyrillic Extended-B
            0x2000, 0x206F, // Punctuation
            0,
        };
        io.Fonts->AddFontFromFileTTF("Assets/Fonts/font.ttf", 15.5f, &fontConfig, ranges);

        if (!std::filesystem::exists("Saves/"))
            std::filesystem::create_directory("Saves");

        mScriptFile.OpenLibraries(sol::lib::base);
        mScriptFile.Init();
        mScriptFile.RequireFile("", "Assets/Scripts/resources.lua");
        mScriptFile.LoadFile("Assets/Scripts/config.lua", false);
        mScriptFile.LoadFile("Assets/Scripts/utils.lua", false);
        mScriptFile.LoadFile("Assets/Scripts/main.lua", false);
        oe::VisualNovel::Init(&mScriptFile);
        Hazel::Audio::SetGlobalVolume(0.5f);

        return true;
    }

    bool Application::OnUpdate(float deltaTime)
    {
        using namespace oe;

        if (mAutoNextStep)
        {
            mAutoSkipTimer.TotalTime += deltaTime;
            if (mAutoSkipTimer.TotalTime >= mAutoSkipTimer.MaxTime)
            {
                VisualNovel::NextStep();
                mAutoSkipTimer.TotalTime = 0.0f;
            }
        }

        Core::Root::GetWorld()->UpdateEntities();
        oe::VisualNovel::Update(deltaTime);

        UpdateSavesMenu(deltaTime);
        UpdateHistoryMenu(deltaTime);
        UpdateDebugInfo(deltaTime);

        if (mShowDemoWindow)
            Renderer::GuiLayer::ShowDemoWindow();

        return true;
    }

    void Application::UpdateSavesMenu(float deltaTime)
    {
        if (mShowSavesMenu)
        {
            using namespace oe;
            using namespace Renderer;

            int it{};
            std::vector<std::filesystem::path> saves{};
            const std::string fileName{"player_save"};
            const auto& path = std::filesystem::directory_iterator("Saves/");

            for (auto& file : path)
                saves.push_back(file.path());
            std::sort(saves.begin(), saves.end());

            for (auto& save : saves)
            {
                std::string saveFile = save.filename().replace_extension().string();
                saveFile.erase(saveFile.end() - 1);
                if (saveFile == "player_save")
                    it++;
            }

            GuiLayer::Begin("Saves");
            if (GuiLayer::Button("Save"))
            {
                if (it == 10)
                    GuiLayer::OpenPopup("Overwrite Save");
                else
                    VisualNovel::Save("Saves/" + fileName + std::to_string(it), fileName + std::to_string(it));
            }

            if (GuiLayer::BeginListBox("Saves List", ImVec2(-FLT_MIN, GuiLayer::GetWindowHeight())))
            {
                for (uint32_t i{}; i < saves.size(); ++i)
                {
                    auto& save = saves[i];
                    if (!save.empty())
                    {
                        const bool isSelected = (mSelectedSave == i);
                        if (ImGui::Selectable(save.c_str(), isSelected))
                        {
                            const std::string fileName = save.replace_extension();
                            if (!VisualNovel::LoadSave(&mScriptFile, fileName))
                                OE_LOG_WARNING("Failed to load world '" + fileName + "'!")
                            mShowSavesMenu = false;
                            break;
                        }
                    }
                }
                GuiLayer::EndListBox();
            }

            const ImVec2 center = GuiLayer::GetMainViewport()->GetCenter();
            GuiLayer::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            if (GuiLayer::BeginPopupModal("Overwrite Save", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                for (uint32_t i{}; i < saves.size(); ++i)
                {
                    std::string saveFile = saves[i].filename().replace_extension().string();
                    saveFile.erase(saveFile.end() - 1);
                    if (saveFile != "player_save")
                        saves.erase(saves.begin() + i);
                }

                static uint32_t selected{};
                if (GuiLayer::BeginCombo("##", saves[selected].c_str()))
                {
                    for (uint32_t i{}; i < saves.size(); i++)
                    {
                        const bool isSelected = (selected == i);
                        if (GuiLayer::Selectable(saves[i].c_str(), isSelected))
                            selected = i;

                        if (isSelected)
                            GuiLayer::SetItemDefaultFocus();
                    }
                    GuiLayer::EndCombo();
                }

                GuiLayer::Separator();

                if (GuiLayer::Button("OK", ImVec2(120, 0)))
                {
                    oe::VisualNovel::Save("Saves/" + fileName + std::to_string(selected), fileName + std::to_string(selected));
                    GuiLayer::CloseCurrentPopup();
                }
                GuiLayer::SetItemDefaultFocus();
                GuiLayer::SameLine();
                if (GuiLayer::Button("Cancel", ImVec2(120, 0)))
                    GuiLayer::CloseCurrentPopup();
                GuiLayer::EndPopup();
            }

            GuiLayer::End();
        }
    }

    void Application::UpdateHistoryMenu(float deltaTime)
    {
        const auto& window = oe::Core::Root::GetWindow();
        if (mShowHistoryMenu)
        {
            using namespace oe::Renderer;

            GuiLayer::Begin("History");
            GuiLayer::Text("Press 'ESC' to close!");

            if (GuiLayer::BeginListBox("HistoryList", ImVec2(-FLT_MIN, GuiLayer::GetWindowHeight() / 1.25f)))
            {
                const auto currentIt = oe::VisualNovel::GetCurrentIterator();
                static auto prevIt = currentIt;
                for (uint32_t i{}; i < currentIt; ++i)
                {
                    const auto& instructions = oe::VisualNovel::GetInstructions();
                    const auto& instruction = instructions[i];
                    if (instructions[i].EqualType(oe::VisualNovel::SAY_TEXT))
                    {
                        std::string text{};
                        if (instruction.characterData.character->GetName().empty())
                            text = instruction.characterData.text;
                        else
                            text = instruction.characterData.character->GetName() + ": " + instruction.characterData.text;
                        std::u8string u8String{text.begin(), text.end()};
                        GuiLayer::TextWrapped("%s", u8String.c_str());
                        GuiLayer::Separator();
                    }
                }
                if (prevIt != currentIt)
                    GuiLayer::SetScrollY(GuiLayer::GetWindowHeight() * GuiLayer::GetWindowHeight());
                GuiLayer::EndListBox();
            }
            GuiLayer::End();
        }
        else
        {
            using namespace oe::Renderer;
            GuiLayer::Begin("HistoryMenuButton", nullptr,
                            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground |
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
            if (GuiLayer::Button("Open history", ImVec2(100, 40)))
                mShowHistoryMenu = true;
            GuiLayer::SetWindowPos("HistoryMenuButton", ImVec2(0, 0));
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
            auto currentIt = VisualNovel::GetCurrentIterator();

            GuiLayer::Begin("Debug Info");

            GuiLayer::Text("Frame Time: %.2fms", (deltaTime * 1000));
            GuiLayer::Text("Frames: %.2f", io.Framerate);

            GuiLayer::Text("Window cursor pos: %.f / %.f", windowCursorPosX, windowCursorPosY);
            GuiLayer::Text("GL cursor pos: %.2f / %.2f", glCursorPosX, glCursorPosY);

            GuiLayer::Text("Current label: %s", currentLabel.c_str());
            GuiLayer::Text("Current iterator: %i", currentIt);

            GuiLayer::Text("Is render choice menu: %i", VisualNovel::IsRenderChoiceMenu());

            if (GuiLayer::Button("Show Demo Window"))
                mShowDemoWindow = !mShowDemoWindow;

            GuiLayer::DragFloat("Auto Skip Time", &mAutoSkipTimer.MaxTime, 0.01f, 0.0f, 5.0f);

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
                            title += "Change Scene | " + instruction.sceneSprite->GetTexture()->GetData()->Path;
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
                        case VisualNovel::MOVE_SPRITE: {
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
                                {
                                    title += "var = " + item + "; ";
                                }
                                else
                                {
                                    title += "target = " + item + "; ";
                                }
                                iter++;
                            }
                            break;
                        }
                        case VisualNovel::SET_TEXT_SPEED: break;
                        case VisualNovel::SHOW_TEXTBOX: title += "Show Text Box | " + std::to_string(instruction.animationSpeed); break;
                        case VisualNovel::HIDE_TEXTBOX: title += "Hide Text Box | " + std::to_string(instruction.animationSpeed); break;
                        case VisualNovel::WAIT:
                            title += "Wait | " + std::to_string(instruction.animationSpeed) +
                                     (instruction.Target.empty() ? "" : " / " + instruction.Target);
                            break;
                        }
                        if (ImGui::Selectable(title.c_str(), isSelected))
                        {
                            currentIt = i;
                            uint32_t tempIt{};
                            if (currentIt != instructions.size() && instructions[currentIt + 1].type != VisualNovel::CHOICE_MENU)
                            {
                                if (currentIt > 0 && instructions[currentIt - 1].type == VisualNovel::SAY_TEXT)
                                {
                                    tempIt = currentIt--;
                                }

                                while (true)
                                {
                                    if (currentIt == -1)
                                        break;
                                    if (currentIt > 0 && instructions[currentIt - 1].type != VisualNovel::SAY_TEXT)
                                        currentIt--;
                                    else
                                        break;
                                }
                            }

                            while (true)
                            {
                                if (currentIt == -1)
                                    break;
                                if (currentIt > 0 && instructions[currentIt - 1].EqualType(VisualNovel::HIDE_CHARACTER))
                                    currentIt--;
                                else
                                    break;
                            }

                            VisualNovel::SetCurrentIterator(currentIt);
                            VisualNovel::SetRenderChoiceMenu(false);
                            VisualNovel::NextStep();

                            if (tempIt)
                            {
                                const auto temp = currentIt;
                                VisualNovel::SetCurrentIterator(tempIt);
                                VisualNovel::NextStep();
                                VisualNovel::SetCurrentIterator(temp);
                            }
                        }

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
                            GuiLayer::Text("Alpha: %.2f", sprite->GetAlpha());
                            GuiLayer::Text("IsKeepAr: %i", sprite->IsKeepAR());
                            GuiLayer::Text("Texture path: %s", sprite->GetTexture()->GetData()->Path.c_str());
                            GuiLayer::Text("Texture width: %i", sprite->GetTexture()->GetData()->Width);
                            GuiLayer::Text("Texture height: %i", sprite->GetTexture()->GetData()->Height);
                            GuiLayer::Text("Texture channels: %i", sprite->GetTexture()->GetData()->Channels);
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

    void Application::PushBackgroundInfo(const std::string& title, const oe::World::Entity& background)
    {
        using namespace oe;
        using namespace Renderer;
        if (GuiLayer::TreeNode(title.c_str()))
        {
            if (GuiLayer::TreeNode("Sprite2DComponent"))
            {
                const auto& sprite = background.GetComponent<Sprite2DComponent>().Sprite2D;
                GuiLayer::Text("Alpha: %.2f", sprite->GetAlpha());
                GuiLayer::Text("IsKeepAr: %i", sprite->IsKeepAR());
                GuiLayer::Text("Texture path: %s", sprite->GetTexture()->GetData()->Path.c_str());
                GuiLayer::Text("Texture width: %i", sprite->GetTexture()->GetData()->Width);
                GuiLayer::Text("Texture height: %i", sprite->GetTexture()->GetData()->Height);
                GuiLayer::Text("Texture channels: %i", sprite->GetTexture()->GetData()->Channels);
                GuiLayer::TreePop();
            }

            if (GuiLayer::TreeNode("AnimationComponent"))
            {
                const auto& animation =
                    (Animation::DissolveAnimation<Renderer::GL::Sprite2D>*)background.GetComponent<AnimationComponent>().Animation;
                GuiLayer::Text("Is ended: %i", animation->IsEnded());
                GuiLayer::Text("Is reversed: %i", animation->IsReversed());
                GuiLayer::Text("Total time: %.2f", animation->GetTotalTime());
                GuiLayer::Text("Speed: %.2f", animation->GetSpeed());
                GuiLayer::TreePop();
            }
            GuiLayer::TreePop();
        }
    }

    void Application::OnEvent(const oe::Core::Event::Base& e)
    {
        using namespace oe;
        using namespace oe::Core;
        if (typeid(Event::MouseButtonEvent) == typeid(e))
        {
            const auto& mouseButtonEvent = dynamic_cast<const Event::MouseButtonEvent&>(e);
            if (mouseButtonEvent.Button == Input::LEFT && mouseButtonEvent.Action == Input::PRESS)
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
                case Input::S: {
                    mShowSavesMenu = !mShowSavesMenu;
                    return;
                }
                case Input::H: mShowHistoryMenu = !mShowHistoryMenu; return;
                case Input::J: mAutoNextStep = !mAutoNextStep; return;
                case Input::ESC: mShowHistoryMenu = false; return;
                default: return;
                }
            }
        }
    }

    void Application::OnShutdown()
    {
        oe::VisualNovel::Shutdown();
    }
} // namespace TSOCA

namespace oe::Runtime
{
    std::shared_ptr<Application> CreateApplication(int, char*[])
    {
        return std::make_shared<TSOCA::Application>("The Suffering Of Carl Allen", 1280, 720);
    }
} // namespace oe::Runtime