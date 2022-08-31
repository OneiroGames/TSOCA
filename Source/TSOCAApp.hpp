//
// Copyright (c) Oneiro Games. All rights reserved.
// Licensed under the GNU General Public License, Version 3.0.
//

#pragma once

#include "HazelAudio/HazelAudio.h"
#include "Oneiro/Lua/LuaFile.hpp"
#include "Oneiro/Renderer/OpenGL/Texture.hpp"
#include "Oneiro/Runtime/Application.hpp"
#include "Oneiro/World/World.hpp"
#include "imconfig.h"
#include "imgui.h"
#include <filesystem>

namespace oe::Renderer::GuiLayer
{
    inline void HelpMarker(const std::string& desc);
    inline void AlignForWidth(float width, float alignment = 0.5f);
    inline void AlignText(const std::initializer_list<std::string>& names, float widthOffset);
    inline void CreateSavesPopupModal(std::vector<std::filesystem::path>& saves, const std::string& id,
                                      const std::function<void(std::vector<std::filesystem::path>&)>& saveFilesFunc,
                                      const std::function<void(uint32_t& selected)>& okFunc,
                                      ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize);
} // namespace oe::Renderer::GuiLayer

namespace TSOCA
{
    class Application final : public oe::Runtime::Application
    {
        using oe::Runtime::Application::Application;

      public:
        bool OnPreInit() override;

        bool OnInit() override;

        bool OnUpdate(float deltaTime) override;

        void OnEvent(const oe::Core::Event::Base& e) override;

        void OnShutdown() override;

      private:
        void LoadGuiFont();
        void SetupGuiStyle();
        void InitScripts();
        void SetMonitorFromConfig();
        void SetFullScreenFromConfig();
        void LoadWindowIcon();

        void UpdateMainMenu(float deltaTime);
        void UpdateSavesMenu(float deltaTime);
        void UpdateHistoryMenu(float deltaTime);
        void UpdateDebugInfo(float deltaTime);
        void UpdateSettingsMenu(float deltaTime);
        void UpdateEscapeMenu(float deltaTime);

        void ProcessVnWaiting(float deltaTime);

        void RenderAcceptPopupModal(const std::string& selectedSave, const ImVec2& center);

        void PushBackgroundInfo(const std::string& title, const oe::World::Entity& background);

        void SaveSpecifications();

        struct ConfigData
        {
            const std::string fileName{"config.yaml"};
            GLFWmonitor* windowMonitor{};
            std::string windowMonitorName{};
            float audioVolume{0.45f};
            float textSpeed{80.0f};
            float autoSkipTime{0.05};
            bool windowFullScreen{true};
            bool autoScrollHistory{true};
            bool renderAcceptPopupModal{true};

            [[nodiscard]] bool IsFileExists() const;
            void Save();
            void Load();
        };

        // clang-format off
        const std::vector<glm::vec2> mParticlePositions = {
            {0.0f, 0.0f}, // skip
            {0.0f, 0.5f},
            {0.025f, 0.525f}, {-0.025f, 0.525f},
            {0.05f, 0.55f}, {-0.05f, 0.55f},

            {0.07f, 0.6f}, {-0.07f, 0.6f},

            {0.09f, 0.625f}, {-0.09f, 0.625f},
            {0.11f, 0.625f}, {-0.11f, 0.625f},
            {0.13f, 0.625f}, {-0.13f, 0.625f},
            {0.15f, 0.625f}, {-0.15f, 0.625f},

            {0.17f, 0.6f}, {-0.17f, 0.6f},
            {0.17f, 0.575f}, {-0.17f, 0.575f},
            {0.17f, 0.55f}, {-0.17f, 0.55f},

            {0.18f, 0.525f}, {-0.18f, 0.525f},

            {0.17f, 0.5f}, {-0.17f, 0.5f},
            {0.16f, 0.475f}, {-0.16f, 0.475f},
            {0.15f, 0.45f}, {-0.15f, 0.45f},
            {0.14f, 0.425f}, {-0.14f, 0.425f},
            {0.13f, 0.4f}, {-0.13f, 0.4f},
            {0.12f, 0.375f}, {-0.12f, 0.375f},
            {0.11f, 0.35f}, {-0.11f, 0.35f},
            {0.10f, 0.325f}, {-0.10f, 0.325f},
            {0.09f, 0.3f}, {-0.09f, 0.3f},
            {0.08f, 0.275f}, {-0.08f, 0.275f},
            {0.07f, 0.25f}, {-0.07f, 0.25f},
            {0.055f, 0.225f}, {-0.055f, 0.225f},
            {0.035f, 0.2f}, {-0.035f, 0.2f},
            {0.015f, 0.175f}, {-0.015f, 0.175f},

            {0.0f, 0.15f}
        };
        // clang-format on

        ConfigData mConfigData{};

        oe::Lua::File mScriptFile{};
        Hazel::Audio::Source mMainMenuMusic{};

        float mAutoSkipTotalTime{};
        uint32_t mCurrentParticlePos{};
        uint32_t mSelectedSave{};

        bool mIsStart{true};
        bool mShowDebugInfoMenu{};
        bool mShowSettingsMenu{};
        bool mShowHistoryMenu{};
        bool mShowEscapeMenu{};
        bool mShowDemoWindow{};
        bool mShowSavesMenu{};
        bool mShowAcceptPopupModal{};
        bool mAutoNextStep{};
    };
} // namespace TSOCA