/**
 * Validate snapshot from C++ collector
 * Matches the GameSnapshot structure from snapshot.h
 */
export function validateSnapshot(v) {
  if (!v || typeof v !== 'object' || Array.isArray(v)) {
    return 'snapshot must be an object';
  }

  // Validate timestamp
  if (!Number.isInteger(v.timestamp) || v.timestamp < 0) {
    return 'invalid timestamp';
  }

  // Validate frame number
  if (!Number.isInteger(v.frameNumber) || v.frameNumber < 0) {
    return 'invalid frameNumber';
  }

  // Validate isValid flag
  if (typeof v.isValid !== 'boolean') {
    return 'invalid isValid flag';
  }

  // Validate gameState
  if (!v.gameState || typeof v.gameState !== 'object') {
    return 'missing gameState';
  }

  const gs = v.gameState;
  if (!Number.isInteger(gs.gamePhase) || gs.gamePhase < 0 || gs.gamePhase > 4) {
    return 'invalid gamePhase';
  }

  if (!Number.isInteger(gs.roundNumber) || gs.roundNumber < 0) {
    return 'invalid roundNumber';
  }

  if (typeof gs.mapName !== 'string' || gs.mapName.length > 64) {
    return 'invalid mapName';
  }

  // Validate viewMatrix (16 floats)
  if (!Array.isArray(v.viewMatrix) || v.viewMatrix.length !== 16) {
    return 'invalid viewMatrix';
  }

  for (const val of v.viewMatrix) {
    if (typeof val !== 'number' || !Number.isFinite(val)) {
      return 'viewMatrix contains invalid values';
    }
  }

  // Validate players array
  if (!Array.isArray(v.players) || v.players.length > 64) {
    return 'invalid players array';
  }

  const ids = new Set();
  for (const p of v.players) {
    if (!p || typeof p !== 'object') {
      return 'invalid player object';
    }

    // Validate entity ID
    if (!Number.isInteger(p.entityId) || p.entityId < 0 || ids.has(p.entityId)) {
      return 'invalid player entityId';
    }
    ids.add(p.entityId);

    // Validate name
    if (typeof p.name !== 'string' || p.name.length > 128) {
      return 'invalid player name';
    }

    // Validate team (2 = T, 3 = CT)
    if (!Number.isInteger(p.team) || (p.team !== 0 && p.team !== 2 && p.team !== 3)) {
      return 'invalid player team';
    }

    // Validate health and armor
    if (!Number.isInteger(p.health) || p.health < 0 || p.health > 500) {
      return 'invalid player health';
    }

    if (!Number.isInteger(p.armor) || p.armor < 0 || p.armor > 100) {
      return 'invalid player armor';
    }

    // Validate position
    if (!p.position || typeof p.position !== 'object') {
      return 'missing player position';
    }

    if (!Number.isFinite(p.position.x) || !Number.isFinite(p.position.y) || !Number.isFinite(p.position.z)) {
      return 'invalid player position coordinates';
    }

    // Check for reasonable coordinate ranges
    if (Math.abs(p.position.x) > 100000 || Math.abs(p.position.y) > 100000 || Math.abs(p.position.z) > 100000) {
      return 'player position out of bounds';
    }

    // Validate boolean flags
    if (typeof p.isAlive !== 'boolean') {
      return 'invalid player isAlive';
    }

    if (typeof p.isLocalPlayer !== 'boolean') {
      return 'invalid player isLocalPlayer';
    }

    // Optional: validate screen coordinates if present
    if (p.screenX !== undefined && (!Number.isFinite(p.screenX) || p.screenX < 0 || p.screenX > 1)) {
      return 'invalid player screenX';
    }

    if (p.screenY !== undefined && (!Number.isFinite(p.screenY) || p.screenY < 0 || p.screenY > 1)) {
      return 'invalid player screenY';
    }
  }

  return null;
}
