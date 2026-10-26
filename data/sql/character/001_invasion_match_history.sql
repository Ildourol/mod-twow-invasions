-- ============================================================================
-- mod-twow-invasions: Match History Schema (character database)
-- ============================================================================

CREATE TABLE IF NOT EXISTS `character_invasion_match` (
    `id`       INT(10) UNSIGNED NOT NULL AUTO_INCREMENT,
    `invader`  INT(10) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'characters.guid; 0 once deleted',
    `defender` INT(10) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'characters.guid; 0 once deleted',
    `winner`   INT(10) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'invader or defender, or 0 for draw/forfeit',
    `ended_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_winner` (`winner`),
    KEY `idx_invader` (`invader`),
    KEY `idx_defender` (`defender`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
