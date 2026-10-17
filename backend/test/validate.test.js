import test from 'node:test';
import assert from 'node:assert/strict';
import { validateSnapshot } from '../src/validate.js';
const good = { schemaVersion: 1, source: 'simulated', timestamp: 123, map: 'de_mirage', status: 'SIMULATION', players: [{ id: 1, name: 'Aster', team: 'CT', health: 100, x: 50, y: 40, alive: true, kills: 1, deaths: 0 }] };
test('accepts a normalized snapshot', () => assert.equal(validateSnapshot(good), null));
test('rejects unsupported schemas', () => assert.equal(validateSnapshot({ ...good, schemaVersion: 9 }), 'unsupported schemaVersion'));
test('rejects out of range coordinates', () => assert.equal(validateSnapshot({ ...good, players: [{ ...good.players[0], x: 101 }] }), 'invalid coordinates'));
test('rejects duplicate ids', () => assert.equal(validateSnapshot({ ...good, players: [good.players[0], good.players[0]] }), 'invalid player id'));
test('rejects excessive player count', () => assert.equal(validateSnapshot({ ...good, players: Array(65).fill(good.players[0]).map((p, id) => ({ ...p, id })) }), 'invalid players'));
