enum CompanionScene {
  none,
  wave,
  hug,
  holdHands,
  kiss,
  bow,
  shyLeanIn,
}

extension CompanionSceneExt on CompanionScene {
  String get label => switch (this) {
        CompanionScene.none => 'None',
        CompanionScene.wave => 'Wave',
        CompanionScene.hug => 'Hug',
        CompanionScene.holdHands => 'Hold hands',
        CompanionScene.kiss => 'Kiss',
        CompanionScene.bow => 'Bow',
        CompanionScene.shyLeanIn => 'Shy lean-in',
      };

  String get command => switch (this) {
        CompanionScene.none => 'none',
        CompanionScene.wave => 'wave',
        CompanionScene.hug => 'hug',
        CompanionScene.holdHands => 'hold_hands',
        CompanionScene.kiss => 'kiss',
        CompanionScene.bow => 'bow',
        CompanionScene.shyLeanIn => 'shy_lean_in',
      };

  String get description => switch (this) {
        CompanionScene.none => 'No special scene. Let the model rest in its default pose.',
        CompanionScene.wave => 'Friendly hello pose with one lifted arm.',
        CompanionScene.hug => 'Two tiny figures lean inward for a soft embrace.',
        CompanionScene.holdHands => 'A side-by-side pair reaching together in the middle.',
        CompanionScene.kiss => 'A duo scene with a tiny heart hovering above them.',
        CompanionScene.bow => 'One character dips forward like a little greeting.',
        CompanionScene.shyLeanIn => 'A sweet hesitant duo pose with tucked posture.',
      };

  bool get isDuo => switch (this) {
        CompanionScene.none => false,
        CompanionScene.wave => false,
        CompanionScene.bow => false,
        CompanionScene.hug => true,
        CompanionScene.holdHands => true,
        CompanionScene.kiss => true,
        CompanionScene.shyLeanIn => true,
      };

  bool get isDeviceSupported => false;
}

CompanionScene companionSceneFromCommand(String? command) {
  final normalized = command?.trim().toLowerCase();
  return CompanionScene.values.firstWhere(
    (scene) => scene.command == normalized,
    orElse: () => CompanionScene.none,
  );
}
