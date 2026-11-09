import { test } from "node:test";
import assert from "node:assert/strict";
import { keyNeed } from "./quest-key-policy.mjs";

test("optional is only assigned when evidence refers to the requested key", () => {
  assert.equal(keyNeed("The Dorm room 206 key is required to unlock the room.", "Dorm room 206 key"), "required");
  assert.equal(keyNeed("The Dorm room 206 key is optional.", "Dorm room 206 key"), "optional");
  assert.equal(keyNeed("Use the west stairwell.", "Dorm room 206 key"), "unknown");
  assert.equal(keyNeed("The other objective is optional. The Dorm room 206 key is required.", "Dorm room 206 key"), "required");
  assert.equal(keyNeed("Only one of the keys is needed elsewhere. The Dorm room 206 key is required.", "Dorm room 206 key"), "required");
  assert.equal(keyNeed("The Dorm room 206 key opens the room.", "Dorm room 206 key"), "unknown");
  assert.equal(keyNeed("No information.", ""), "unknown");
});
