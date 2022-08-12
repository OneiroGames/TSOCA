//
// Copyright (c) Oneiro Games. All rights reserved.
// Licensed under the GNU General Public License, Version 3.0.
//

#pragma once

#include "Oneiro/Lua/LuaFile.hpp"
#include "Oneiro/Runtime/Application.hpp"
#include "Oneiro/World/World.hpp"

namespace TSOCA
{
    class Application final : public oe::Runtime::Application
    {
        using oe::Runtime::Application::Application;
        struct AutoSkipTimer
        {
            float TotalTime{};
            float MaxTime{0.05f};
        };

      public:
        bool OnInit() override;
        bool OnUpdate(float deltaTime) override;

        void OnEvent(const oe::Core::Event::Base& e) override;

        void OnShutdown() override;

      private:
        void PushBackgroundInfo(const std::string& title, const oe::World::Entity& background);
        void UpdateSavesMenu(float deltaTime);
        void UpdateHistoryMenu(float deltaTime);
        void UpdateDebugInfo(float deltaTime);

        oe::Lua::File mScriptFile{};
        AutoSkipTimer mAutoSkipTimer{};
        uint32_t mSelectedSave{};
        bool mShowSavesMenu{};
        bool mShowHistoryMenu{};
        bool mShowDebugInfoMenu{};
        bool mShowDemoWindow{};
        bool mAutoNextStep{};
    };
} // namespace TSOCA