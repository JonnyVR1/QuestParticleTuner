#include <algorithm>

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/utils.h"
#include "UnityEngine/ParticleSystem.hpp"
#include "UnityEngine/ParticleSystem_MainModule.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/Color32.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "GlobalNamespace/NoteCutParticlesEffect.hpp"
#include "GlobalNamespace/SaberClashEffect.hpp"
#include "UnityEngine/Random.hpp"
#include "particletune.hpp"
#include "particletune_private.hpp"
#include "PTScenePSController.hpp"
#include "Config.hpp"
#include "UnityInternalCalls.hpp"

using namespace ParticleTuner;

MAKE_HOOK_OFFSETLESS(NoteCutParticlesEffect_SpawnParticles, void,
                     GlobalNamespace::NoteCutParticlesEffect* self, Vector3 pos, Vector3 cutNormal,
                     Vector3 saberDir, Vector3 moveVec, Color32 color,
                     int sparkleParticlesCount, int explosionParticlesCount,
                     float lifetimeMultiplier)
{
    auto& currentConfig = getConfig();
    int newSparkleCount = static_cast<int>(sparkleParticlesCount * currentConfig.sparkleMultiplier);
    int newExplosionCount = static_cast<int>(explosionParticlesCount * currentConfig.explosionMultiplier);
    float newLifetimeMultiplier = lifetimeMultiplier * currentConfig.lifetimeMultiplier;

    auto sparklesPS = self->sparklesPS;
    auto explosionPS = self->explosionPS;
    auto corePS = self->explosionCorePS;

    auto sparklesMain = sparklesPS->get_main();
    auto explosionMain = explosionPS->get_main();
    auto coreMain = corePS->get_main();

    sparklesMain.set_maxParticles(INT_MAX);
    explosionMain.set_maxParticles(INT_MAX);
    coreMain.set_maxParticles(currentConfig.reduceCoreParticles ? 0 : INT_MAX);

    icall_functions::ParticleSystem_MainModule::set_startLifetimeMultiplier_Injected(
        &explosionMain,
        currentConfig.lifetimeMultiplier
    );

    icall_functions::ParticleSystem_MainModule::set_startLifetimeMultiplier_Injected(
        &coreMain,
        currentConfig.reduceCoreParticles ? 0.0f : 1.0f
    );

    if (currentConfig.rainbowParticles) {
        auto randomColor = UnityEngine::Random::ColorHSV(0, 1, 1, 1, 1, 1, 1, 1);
        color.r = static_cast<uint8_t>(randomColor.r * 255);
        color.g = static_cast<uint8_t>(randomColor.g * 255);
        color.b = static_cast<uint8_t>(randomColor.b * 255);
    }

    // Set alpha channel
    color.a = static_cast<uint8_t>(std::clamp(currentConfig.particleOpacity * 255.0f, 0.0f, 255.0f));
    
    NoteCutParticlesEffect_SpawnParticles(self, pos, cutNormal, saberDir, moveVec, color, newSparkleCount, newExplosionCount, newLifetimeMultiplier);
}

MAKE_HOOK_OFFSETLESS(SaberClashEffect_LateUpdate, void,
                     GlobalNamespace::SaberClashEffect* self)
{
    auto& currentConfig = getConfig();

    auto glowPS = self->glowParticleSystem;
    auto sparklePS = self->sparkleParticleSystem;

    glowPS->get_main().set_maxParticles(currentConfig.reduceClashParticles ? 0 : INT_MAX);
    sparklePS->get_main().set_maxParticles(currentConfig.reduceClashParticles ? 0 : INT_MAX);
    
    SaberClashEffect_LateUpdate(self);
}

MAKE_HOOK_OFFSETLESS(SceneManager_Internal_ActiveSceneChanged, void,
                     UnityEngine::SceneManagement::Scene oldScene,
                     UnityEngine::SceneManagement::Scene newScene
) {
    static std::vector<std::string> sensitiveScenes = {
        "Init", "MenuViewControllers", "GameCore", "Credits"
    };

    SceneManager_Internal_ActiveSceneChanged(oldScene, newScene);

    auto sceneNameCs = newScene.get_name();

    if (sceneNameCs) {
        auto sceneName = to_utf8(csstrtostr(newScene.get_name()));
        // getLogger().info("Transitioning to scene: %s", sceneName.data());
        
        if (std::find(sensitiveScenes.begin(), sensitiveScenes.end(), sceneName) != sensitiveScenes.end()) {
            getLogger().info("Starting PTScenePSDiscoveryAgent...");
            auto discoveryAgent = THROW_UNLESS(il2cpp_utils::New<PTScenePSDiscoveryAgent*>());
            GlobalNamespace::SharedCoroutineStarter::get_instance()
                ->StartCoroutine(reinterpret_cast<System::Collections::IEnumerator*>(discoveryAgent));
        }
    }
}

void PTInstallHooks() {
    getLogger().info("Adding hooks...");
    INSTALL_HOOK_OFFSETLESS(NoteCutParticlesEffect_SpawnParticles, il2cpp_utils::FindMethodUnsafe("", "NoteCutParticlesEffect", "SpawnParticles", 8));
    INSTALL_HOOK_OFFSETLESS(SaberClashEffect_LateUpdate, il2cpp_utils::FindMethod("", "SaberClashEffect", "LateUpdate"));
    INSTALL_HOOK_OFFSETLESS(SceneManager_Internal_ActiveSceneChanged, il2cpp_utils::FindMethodUnsafe("UnityEngine.SceneManagement", "SceneManager", "Internal_ActiveSceneChanged", 2));
}
