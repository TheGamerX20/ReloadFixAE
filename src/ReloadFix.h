#pragma once

namespace RE
{
	class hkbClipGenerator
	{
	public:
		// members
		std::uint64_t		unk00[0x38 >> 3];			// 00
		RE::hkStringPtr		animName;					// 38
		std::uint64_t		unk40[(0x90 - 0x40) >> 3];	// 40
		RE::hkStringPtr		animPath;					// 90
	};

	class hkbBehaviorGraph
	{
	public:
		struct NodeData
		{
			hkbClipGenerator* clipGenerator;	// 00
			hkbClipGenerator* clipGenerator2;	// 08
			hkbBehaviorGraph* behaviorGraph;	// 10
		};

		// members
		std::uint64_t			unk00[0xE0 >> 3];				// 000
		RE::hkArray<NodeData*>* activeNodes;					// 0E0
		std::uint64_t			unkE8[(0x1A8 - 0xE8) >> 3];		// 0E8
		std::uint8_t			unk1A8;							// 1A8
		std::uint8_t			unk1A9;							// 1A9
		bool					isActive;						// 1AA
		bool					isLinked;						// 1AB
		bool					updateActiveNodes;				// 1AC
		bool					stateOrTransitionChanged;		// 1AD
	};

	class BShkbAnimationGraph
	{
	public:
		// members
		std::uint64_t									unk00;							// 000
		RE::BSIntrusiveRefCounted						refCount;						// 008
		RE::BSTEventSource<RE::BSTransformDeltaEvent>	deltaEvent;						// 010
		RE::BSTEventSource<RE::BSAnimationGraphEvent>	animGraphEvent;					// 068
		std::uint64_t									unkC0[(0x378 - 0xC0) >> 3];		// 0C0
		hkbBehaviorGraph*								behaviourGraph;					// 378
	};
}

namespace ReloadFix
{
	static REX::TOML::Bool bPreventTogglePOVDuringReload{ "Configs"sv, "bPreventTogglePOVDuringReload"sv, true };
	static REX::TOML::Bool bPreventReloadAfterTogglePOV{ "Configs"sv, "bPreventReloadAfterTogglePOV"sv, true };
	static REX::TOML::Bool bPreventSprintReloading{ "Configs"sv, "bPreventSprintReloading"sv, true };

	namespace Utils
	{
		inline bool IsSprinting()
		{
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
			if (!player)
				return false;

			return player->moveMode & 0x0100;
		}

		inline bool IsFirstPerson()
		{
			RE::PlayerCamera* playerCamera = RE::PlayerCamera::GetSingleton();
			if (!playerCamera)
				return false;

			return playerCamera->GetCameraCurrentState() == playerCamera->cameraStates[RE::CameraState::kFirstPerson];
		}

		inline bool IsWeaponDrawn()
		{
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
			if (!player)
				return false;

			return player->GetWeaponMagicDrawn();
		}

		inline bool IsVanityModeEnabled()
		{
			RE::PlayerControls* playerControls = RE::PlayerControls::GetSingleton();
			if (!playerControls)
				return false;

			return playerControls->data.vanityModeEnabled;
		}

		inline void ToggleVanityMode(bool enable)
		{
			RE::PlayerControls* playerControls = RE::PlayerControls::GetSingleton();
			if (!playerControls)
				return;

			playerControls->data.vanityModeEnabled = enable;
		}

		inline bool PerformAction(RE::Actor* a_actor, std::uint32_t a_actionIndex, RE::TESObjectREFR* a_ref)
		{
			using func_t = decltype(&PerformAction);
			REL::Relocation<func_t> func{ REL::ID{ 445541, 2231176 } };
			return func(a_actor, a_actionIndex, a_ref);
		}

		inline void ToggleSprint(bool a_sprint)
		{
			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
			if (!player)
				return;

			if (a_sprint && player->stance == 0x01)
				PerformAction(player, 0x30, nullptr);

			player->sprintToggled = a_sprint;
		}

		inline bool IsReloading()
		{
			if (!Utils::IsWeaponDrawn())
				return false;

			RE::PlayerCharacter* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->currentProcess || !player->currentProcess->middleHigh || !player->currentProcess->middleHigh->animationGraphManager)
				return false;

			bool isFirstPerson = Utils::IsFirstPerson();
			RE::BSAnimationGraphManager* animGraphManager = (RE::BSAnimationGraphManager*)player->currentProcess->middleHigh->animationGraphManager.get();
			if ((isFirstPerson && !animGraphManager->variableCache.graphToCacheFor) || animGraphManager->graph.empty())
				return false;

			RE::BShkbAnimationGraph* animGraph = (RE::BShkbAnimationGraph*)(isFirstPerson ? animGraphManager->variableCache.graphToCacheFor.get() : animGraphManager->graph[0].get());
			if (!animGraph)
				return false;

			RE::hkbBehaviorGraph* behaviorGraph = animGraph->behaviourGraph;
			if (!behaviorGraph || !behaviorGraph->activeNodes || behaviorGraph->activeNodes->size() == 0)
				return false;

			if (behaviorGraph->updateActiveNodes || behaviorGraph->stateOrTransitionChanged)
				return false;

			bool wpnReload = false;
			for (std::int32_t ii = 0; ii < behaviorGraph->activeNodes->size(); ii++)
			{
				if (!behaviorGraph->activeNodes->data()[ii]->clipGenerator || !behaviorGraph->activeNodes->data()[ii]->clipGenerator->animName.data())
					continue;

				if (strncmp(behaviorGraph->activeNodes->data()[ii]->clipGenerator->animName.data(), "WPNReload", strlen("WPNReload")) == 0)
					wpnReload = true;

				if (strncmp(behaviorGraph->activeNodes->data()[ii]->clipGenerator->animName.data(), "ReloadEndBlend", strlen("ReloadEndBlend")) == 0)
					return false;
			}

			return wpnReload;
		}
	}

	namespace Hooks
	{
		static REL::Relocation<float*> MinCurrentZoom{ REL::ID{ 1011622, 2664607 } };
		inline bool g_isSprintQueued = false;

		struct FirstPersonState_HandleEvent // FirstPersonState::HandleEvent
		{
			static void thunk(void* a_a1, RE::ButtonEvent* a_event)
			{
				if (Utils::IsReloading())
					return;

				return func(a_a1, a_event);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ThirdPersonState_HandleEvent // ThirdPersonState::HandleEvent
		{
			static void thunk(RE::ThirdPersonState* a_tpState, RE::ButtonEvent* a_event)
			{
				if (a_event->value == 0.0f && a_tpState && !a_tpState->freeRotationEnabled)
					a_tpState->freeRotation.x = 0.0f;

				if (Utils::IsReloading())
				{
					if (a_event->strUserEvent == "TogglePOV")
						return;
					else if (a_event->strUserEvent == "ZoomIn")
					{
						if (a_tpState && a_tpState->currentZoomOffset <= *MinCurrentZoom)
							return;
					}
				}

				return func(a_tpState, a_event);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct ReadyWeaponHandler_HandleEvent // ReadyWeaponHandler::HandleEvent
		{
			static void thunk(void* a_a1, RE::ButtonEvent* a_event)
			{
				func(a_a1, a_event);

				if (Utils::IsSprinting() && Utils::IsReloading())
				{
					g_isSprintQueued = true;
					Utils::ToggleSprint(false);
				}
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct MovementHandler_HandleEvent // MovementHandler::HandleEvent
		{
			static void thunk(void* a_a1, RE::ButtonEvent* a_event)
			{
				if (g_isSprintQueued && (a_event->strUserEvent != "Forward" || a_event->value == 0.0f))
					g_isSprintQueued = false;

				func(a_a1, a_event);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct SprintHandler_HandleEvent // SprintHandler::HandleEvent
		{
			static void thunk(void* a_a1, RE::ButtonEvent* a_event)
			{
				if (Utils::IsReloading())
				{
					g_isSprintQueued = true;
					return;
				}

				func(a_a1, a_event);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BSAnimationGraphEvent_ProcessEvent // PlayerCharacter's Actor::ProcessEvent for BSAnimationGraphEvent
		{
			static RE::BSEventNotifyControl thunk(void* a_a1, RE::BSAnimationGraphEvent* a_event, void* a_a3)
			{
				if (a_event->tag == "reloadState" && a_event->payload == "Exit")
				{
					if (g_isSprintQueued)
					{
						Utils::ToggleSprint(true);
						g_isSprintQueued = false;
					}
				}

				return func(a_a1, a_event, a_a3);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct MenuOpenCloseEvent_ProcessEvent // PlayerControls::ProcessEvent for MenuOpenCloseEvent
		{
			static RE::BSEventNotifyControl thunk(void* a_a1, RE::MenuOpenCloseEvent* a_event, void* a_a3)
			{
				if (a_event->menuName == "LoadingMenu" && a_event->opening)
					g_isSprintQueued = false;

				return func(a_a1, a_event, a_a3);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	inline bool Install()
	{
		if (bPreventTogglePOVDuringReload.GetValue())
		{
			Hooks::FirstPersonState_HandleEvent::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE::FirstPersonState[0] }.write_vfunc(8, Hooks::FirstPersonState_HandleEvent::thunk);
			Hooks::ThirdPersonState_HandleEvent::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE::ThirdPersonState[0] }.write_vfunc(8, Hooks::ThirdPersonState_HandleEvent::thunk);
		}

		if (bPreventSprintReloading.GetValue())
		{
			Hooks::ReadyWeaponHandler_HandleEvent::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE::ReadyWeaponHandler[0] }.write_vfunc(8, Hooks::ReadyWeaponHandler_HandleEvent::thunk);
			Hooks::MovementHandler_HandleEvent::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE::MovementHandler[0] }.write_vfunc(8, Hooks::MovementHandler_HandleEvent::thunk);
			Hooks::SprintHandler_HandleEvent::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE::SprintHandler[0] }.write_vfunc(8, Hooks::SprintHandler_HandleEvent::thunk);
			Hooks::BSAnimationGraphEvent_ProcessEvent::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE::PlayerCharacter[3] }.write_vfunc(1, Hooks::BSAnimationGraphEvent_ProcessEvent::thunk);
			Hooks::MenuOpenCloseEvent_ProcessEvent::func = REL::Relocation<std::uintptr_t>{ RE::VTABLE::PlayerControls[1] }.write_vfunc(1, Hooks::MenuOpenCloseEvent_ProcessEvent::thunk);
		}

		if (bPreventReloadAfterTogglePOV.GetValue())
		{
			if (REX::FModule::IsRuntimeOG())
			{
				uint8_t buffer[] = { 0x40, 0x30, 0xFF, 0x90 };
				REL::WriteSafe(REL::Relocation<uintptr_t>{ REL::ID{ 601228 }, REL::Offset{ 0x446 } }.address(), buffer, sizeof(buffer));
			}
			else
			{
				uint8_t buffer[] = { 0xEB, 0x15 };
				REL::WriteSafe(REL::Relocation<uintptr_t>{ REL::ID{ 2233236 }, REL::Offset{ 0x57B } }.address(), buffer, sizeof(buffer));
			}
		}

		return true;
	}

	inline void F4SEMessageListener(F4SE::MessagingInterface::Message* a_msg)
    {
        if (!a_msg)
			return;

		// Could probably use Event Sinks for BSAnimationGraphEvent and MenuOpenCloseEvent..?
		if (a_msg->type == F4SE::MessagingInterface::kNewGame || a_msg->type == F4SE::MessagingInterface::kPostLoadGame)
		{
			if (bPreventTogglePOVDuringReload.GetValue())
				Hooks::g_isSprintQueued = false;
		}
    }
}
