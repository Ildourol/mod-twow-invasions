-- ============================================================================
-- mod-twow-invasions: World PvP Invasions (World Database Setup)
-- ============================================================================

-- ----------------------------------------------------------------------------
-- 1. The Invader's Sigil (Item 100035) - Optional Physical Talisman
-- ----------------------------------------------------------------------------
DELETE FROM `item_template` WHERE `entry` = 100035;
INSERT INTO `item_template`
  (`entry`, `class`, `subclass`, `name`, `description`, `display_id`, `quality`, `flags`,
   `buy_price`, `sell_price`, `inventory_type`, `allowable_class`, `allowable_race`,
   `item_level`, `required_level`, `stackable`, `max_count`, `bonding`, `material`, `sheath`,
   `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `script_name`)
VALUES
  (100035, 15, 0, 'Invader\'s Sigil',
   'Right-click to hunt down someone flagged for war. You go to them.',
   22952, 3, 0,
   0, 0, 0, -1, -1,
   1, 10, 1, 1, 1, 6, 0,
   51942, 0, 0, 'item_invasion_sigil');

-- ----------------------------------------------------------------------------
-- 2. Spell 51942 (Rite of Invasion) Script Binding & Server Template
-- ----------------------------------------------------------------------------
UPDATE `spell_template` SET `name` = 'Rite of Invasion', `script_name` = 'spell_invasion_search' WHERE `entry` = 51942;

-- ----------------------------------------------------------------------------
-- 3. Mordrin the Marked (Creature 100031) - Talisman Giver NPC
-- ----------------------------------------------------------------------------
DELETE FROM `broadcast_text` WHERE `entry` = 6400041;
INSERT INTO `broadcast_text` (`entry`, `male_text`, `female_text`, `chat_type`, `language_id`) VALUES
  (6400041,
   'You have the look of someone who is owed a fight.$B$BTake a sigil. Burn it when you are ready and it will find you somebody near your own strength -- flagged, willing, and somewhere far from here. It puts you at their back. Not them at yours.$B$BWhat happens after that is between the two of you.',
   '', 0, 0);

DELETE FROM `npc_text` WHERE `ID` = 6400041;
INSERT INTO `npc_text` (`ID`, `BroadcastTextID0`, `Probability0`) VALUES (6400041, 6400041, 1);

DELETE FROM `gossip_menu` WHERE `entry` = 64030;
INSERT INTO `gossip_menu` (`entry`, `text_id`) VALUES (64030, 6400041);

DELETE FROM `gossip_menu_option` WHERE `menu_id` = 64030;
INSERT INTO `gossip_menu_option`
  (`menu_id`, `id`, `option_icon`, `option_text`, `option_id`, `npc_option_npcflag`,
   `action_script_id`, `condition_id`)
VALUES
  (64030, 0, 0, 'Give me a sigil.', 1, 1, 6400241, 6400041);

-- Condition: Player does NOT already possess an Invader's Sigil (flags = 1 reverse result)
DELETE FROM `conditions` WHERE `condition_entry` = 6400041;
INSERT INTO `conditions` (`condition_entry`, `type`, `value1`, `value2`, `value3`, `value4`, `flags`)
VALUES (6400041, 2, 100035, 1, 0, 0, 1);

-- Gossip script: Give 1 Invader's Sigil (command 17 = SCRIPT_COMMAND_CREATE_ITEM)
DELETE FROM `gossip_scripts` WHERE `id` = 6400241;
INSERT INTO `gossip_scripts` (`id`, `delay`, `command`, `datalong`, `datalong2`, `datalong3`, `comments`)
VALUES (6400241, 0, 17, 100035, 1, 0, 'Invasions: give one Invader\'s Sigil');

-- Creature Template: Mordrin the Marked
DELETE FROM `creature_template` WHERE `entry` = 100031;
INSERT INTO `creature_template`
  (`entry`, `name`, `subname`, `display_id1`, `faction`, `npc_flags`, `gossip_menu_id`,
   `level_min`, `level_max`, `health_min`, `health_max`, `armor`, `unit_class`, `type`,
   `speed_walk`, `speed_run`, `scale`, `base_attack_time`, `ranged_attack_time`,
   `dmg_min`, `dmg_max`, `movement_type`, `inhabit_type`, `regeneration`, `ai_name`)
VALUES
  (100031, 'Mordrin the Marked', 'Invasions', 2873, 35, 1, 64030,
   60, 60, 4007, 4007, 3272, 1, 7,
   1.0, 1.14286, 1.0, 2000, 2000,
   100, 150, 0, 3, 1, '');
