#include "pch-il2cpp.h"
#include "RapidFire.h"

#include <helpers.h>
#include <cheat/game/EntityManager.h>
#include <cheat/game/util.h>
#include <cheat/game/filters.h>

namespace cheat::feature
{
	static void LCBaseCombat_DoHitEntity_Hook(app::LCBaseCombat* __this, uint32_t targetID, app::AttackResult* attackResult,
		bool ignoreCheckCanBeHitInMP, MethodInfo* method);
	static void VCAnimatorEvent_HandleProcessItem_Hook(app::MoleMole_VCAnimatorEvent* __this,
		app::MoleMole_VCAnimatorEvent_MoleMole_VCAnimatorEvent_AnimatorEventPatternProcessItem* processItem,
		app::AnimatorStateInfo processStateInfo, app::MoleMole_VCAnimatorEvent_MoleMole_VCAnimatorEvent_TriggerMode__Enum mode, MethodInfo* method);

	RapidFire::RapidFire() : Feature(),
		NF(f_Enabled, "Attack Multiplier", "RapidFire", false),
		NF(f_MultiHit, "Multi-hit", "RapidFire", false),
		NF(f_Multiplier, "Hit Multiplier", "RapidFire", 2),
		NF(f_OnePunch, "One Punch Mode", "RapidFire", false),
		NF(f_Randomize, "Randomize", "RapidFire", false),
		NF(f_minMultiplier, "Min Multiplier", "RapidFire", 1),
		NF(f_maxMultiplier, "Max Multiplier", "RapidFire", 3),
		NF(f_MultiTarget, "Multi-target", "RapidFire", false),
		NF(f_MultiTargetRadius, "Multi-target Radius", "RapidFire", 20.0f),
		NF(f_MultiAnimation, "Multi-animation", "RapidFire", false)
	{
		HookManager::install(app::MoleMole_LCBaseCombat_DoHitEntity, LCBaseCombat_DoHitEntity_Hook);
		HookManager::install(app::MoleMole_VCAnimatorEvent_HandleProcessItem, VCAnimatorEvent_HandleProcessItem_Hook);
	}

	const FeatureGUIInfo& RapidFire::GetGUIInfo() const
	{
		static const FeatureGUIInfo info{ u8"攻击作弊", "Player", true };
		return info;
	}

	void RapidFire::DrawMain()
	{
		ConfigWidget(u8"开/关", f_Enabled, u8"启用攻击倍数。需要选择一种模式。");
		ImGui::SameLine();
		ImGui::TextColored(ImColor(255, 165, 0, 255), u8"请选择其中一个模式.");

		ConfigWidget(u8"多倍伤害模式", f_MultiHit, u8"启用多倍伤害模式。\n" \
			u8"加倍你的攻击次数\n");


		ImGui::Indent();

		ConfigWidget(u8"一拳模式", f_OnePunch, u8"根据敌人的生命值计算杀死敌人所需的攻击次数\n" \
			u8"并使用其相应地设置乘数。\n" \
			u8"可能更安全，但乘数计算可能不正确。");

		ConfigWidget(u8"概率模式", f_Randomize, u8"在最小和最大乘数之间随机化乘数。");
		ImGui::SameLine();
		ImGui::TextColored(ImColor(255, 165, 0, 255), u8"这个和一拳模式是冲突的!");

		if (!f_OnePunch) {
			if (!f_Randomize)
			{
				ConfigWidget(u8"攻击倍数", f_Multiplier, 1, 2, 1000, u8"攻击计数乘数。");
			}
			else
			{
				ConfigWidget(u8"最小倍数", f_minMultiplier, 1, 2, 1000, u8"攻击计数是最小乘数。");
				ConfigWidget(u8"最大倍数", f_maxMultiplier, 1, 2, 1000, u8"攻击计数是最大乘数。");
			}
		}

		ImGui::Unindent();

		ConfigWidget(u8"范围伤害", f_MultiTarget, u8"在指定的目标半径内启用多目标攻击。\n" \
			u8"初始目标周围的所有有效目标将根据设置命中。\n" \
			u8"伤害数字将仅出现在初始目标上，但所有有效目标都已损坏。\n" \
			u8"如果禁用了多倍伤害模式，并且单个目标上仍有多个数字，请检查“调试”部分中的实体管理器，以查看是否存在不可见的实体。\n" \
			u8"如果与多倍伤害模式一起使用，这可能会导致极端滞后和快速禁止。你会被封禁。"
		);

		ImGui::Indent();
		ConfigWidget(u8"半径 (m)", f_MultiTargetRadius, 0.1f, 5.0f, 50.0f, u8"检查有效目标的半径。");
		ImGui::Unindent();

		ConfigWidget(u8"多倍技能", f_MultiAnimation, u8"启用多动画攻击。\n" \
			u8"请记住，角色的音频也会被垃圾邮件。");
	}

	bool RapidFire::NeedStatusDraw() const
	{
		return f_Enabled && (f_MultiHit || f_MultiTarget || f_MultiAnimation);
	}

	void RapidFire::DrawStatus()
	{
		if (f_MultiHit)
		{
			if (f_Randomize)
				ImGui::Text(u8"多倍攻击 随机%d|%d]", f_minMultiplier.value(), f_maxMultiplier.value());
			else if (f_OnePunch)
				ImGui::Text(u8"多倍攻击[一拳模式]");
			else
				ImGui::Text(u8"多倍攻击 [%d]", f_Multiplier.value());
		}
		if (f_MultiTarget)
			ImGui::Text(u8"我的[%.01fm]大刀", f_MultiTargetRadius.value());

		if (f_MultiAnimation)
			ImGui::Text(u8"多倍技能");
	}

	RapidFire& RapidFire::GetInstance()
	{
		static RapidFire instance;
		return instance;
	}


	int RapidFire::CalcCountToKill(float attackDamage, uint32_t targetID)
	{
		if (attackDamage == 0)
			return f_Multiplier;

		auto& manager = game::EntityManager::instance();
		auto targetEntity = manager.entity(targetID);
		if (targetEntity == nullptr)
			return f_Multiplier;

		auto baseCombat = targetEntity->combat();
		if (baseCombat == nullptr)
			return f_Multiplier;

		auto safeHP = baseCombat->fields._combatProperty_k__BackingField->fields.HP;
		auto HP = app::MoleMole_SafeFloat_get_Value(safeHP, nullptr);
		int attackCount = (int)ceil(HP / attackDamage);
		return std::clamp(attackCount, 1, 200);
	}

	int RapidFire::GetAttackCount(app::LCBaseCombat* combat, uint32_t targetID, app::AttackResult* attackResult)
	{
		if (!f_MultiHit)
			return 1;

		auto& manager = game::EntityManager::instance();
		auto targetEntity = manager.entity(targetID);
		auto baseCombat = targetEntity->combat();
		if (baseCombat == nullptr)
			return 1;

		int countOfAttacks = f_Multiplier;
		if (f_OnePunch)
		{
			app::MoleMole_Formula_CalcAttackResult(combat->fields._combatProperty_k__BackingField,
				baseCombat->fields._combatProperty_k__BackingField,
				attackResult, manager.avatar()->raw(), targetEntity->raw(), nullptr);
			countOfAttacks = CalcCountToKill(attackResult->fields.damage, targetID);
		}
		if (f_Randomize)
		{
			countOfAttacks = rand() % (f_maxMultiplier.value() - f_minMultiplier.value()) + f_minMultiplier.value();
			return countOfAttacks;
		}

		return countOfAttacks;
	}

	bool IsAvatarOwner(game::Entity entity)
	{
		auto& manager = game::EntityManager::instance();
		auto avatarID = manager.avatar()->runtimeID();

		while (entity.isGadget())
		{
			game::Entity temp = entity;
			entity = game::Entity(app::MoleMole_GadgetEntity_GetOwnerEntity(reinterpret_cast<app::GadgetEntity*>(entity.raw()), nullptr));
			if (entity.runtimeID() == avatarID)
				return true;
		}

		return false;

	}

	bool IsAttackByAvatar(game::Entity& attacker)
	{
		if (attacker.raw() == nullptr)
			return false;

		auto& manager = game::EntityManager::instance();
		auto avatarID = manager.avatar()->runtimeID();
		auto attackerID = attacker.runtimeID();

		return attackerID == avatarID || IsAvatarOwner(attacker);
	}

	bool IsConfigByAvatar(game::Entity& attacker)
	{
		if (attacker.raw() == nullptr)
			return false;

		auto& manager = game::EntityManager::instance();
		auto avatarID = manager.avatar()->raw()->fields._configID_k__BackingField;
		auto attackerID = attacker.raw()->fields._configID_k__BackingField;
		// Taiga#5555: IDs can be found in ConfigAbility_Avatar_*.json or GadgetExcelConfigData.json
		bool bulletID = attackerID >= 40000160 && attackerID <= 41069999;

		return avatarID == attackerID || bulletID || attacker.type() == app::EntityType__Enum_1::Bullet;
	}

	bool IsValidByFilter(game::Entity* entity)
	{
		if (game::filters::combined::OrganicTargets.IsValid(entity) ||
			game::filters::monster::SentryTurrets.IsValid(entity) ||
			game::filters::combined::Ores.IsValid(entity) ||
			game::filters::puzzle::Geogranum.IsValid(entity) ||
			game::filters::puzzle::LargeRockPile.IsValid(entity) ||
			game::filters::puzzle::SmallRockPile.IsValid(entity))
			return true;
		return false;
	}

	// Raises when any entity do hit event.
	// Just recall attack few times (regulating by combatProp)
	// It's not tested well, so, I think, anticheat can detect it.
	static void LCBaseCombat_DoHitEntity_Hook(app::LCBaseCombat* __this, uint32_t targetID, app::AttackResult* attackResult,
		bool ignoreCheckCanBeHitInMP, MethodInfo* method)
	{
		auto attacker = game::Entity(__this->fields._._._entity);
		RapidFire& rapidFire = RapidFire::GetInstance();
		if (!IsConfigByAvatar(attacker) || !IsAttackByAvatar(attacker) || !rapidFire.f_Enabled)
			return CALL_ORIGIN(LCBaseCombat_DoHitEntity_Hook, __this, targetID, attackResult, ignoreCheckCanBeHitInMP, method);

		auto& manager = game::EntityManager::instance();
		auto originalTarget = manager.entity(targetID);

		if (!IsValidByFilter(originalTarget))
			return CALL_ORIGIN(LCBaseCombat_DoHitEntity_Hook, __this, targetID, attackResult, ignoreCheckCanBeHitInMP, method);

		std::vector<cheat::game::Entity*> validEntities;
		validEntities.push_back(originalTarget);

		if (rapidFire.f_MultiTarget)
		{
			auto filteredEntities = manager.entities();
			for (const auto& entity : filteredEntities) {
				auto distance = originalTarget->distance(entity);

				if (entity->runtimeID() == manager.avatar()->runtimeID())
					continue;

				if (entity->runtimeID() == targetID)
					continue;

				if (distance > rapidFire.f_MultiTargetRadius)
					continue;

				if (!IsValidByFilter(entity))
					continue;

				validEntities.push_back(entity);
			}
		}

		for (const auto& entity : validEntities) {

			if (rapidFire.f_MultiHit) {
				int attackCount = rapidFire.GetAttackCount(__this, entity->runtimeID(), attackResult);
				for (int i = 0; i < attackCount; i++)
					app::MoleMole_LCBaseCombat_FireBeingHitEvent(__this, entity->runtimeID(), attackResult, method);
			}
		}

		CALL_ORIGIN(LCBaseCombat_DoHitEntity_Hook, __this, targetID, attackResult, ignoreCheckCanBeHitInMP, method);
	}

	static void VCAnimatorEvent_HandleProcessItem_Hook(app::MoleMole_VCAnimatorEvent* __this,
		app::MoleMole_VCAnimatorEvent_MoleMole_VCAnimatorEvent_AnimatorEventPatternProcessItem* processItem,
		app::AnimatorStateInfo processStateInfo, app::MoleMole_VCAnimatorEvent_MoleMole_VCAnimatorEvent_TriggerMode__Enum mode, MethodInfo* method)
	{
		auto attacker = game::Entity(__this->fields._._._entity);
		RapidFire& rapidFire = RapidFire::GetInstance();

		if (rapidFire.f_MultiAnimation && IsAttackByAvatar(attacker))
			processItem->fields.lastTime = 0;

		CALL_ORIGIN(VCAnimatorEvent_HandleProcessItem_Hook, __this, processItem, processStateInfo, mode, method);
	}
}