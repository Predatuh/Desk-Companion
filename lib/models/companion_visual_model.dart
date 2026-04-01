enum CompanionVisualModel { classic, stickFigure, robot }

extension CompanionVisualModelExt on CompanionVisualModel {
  String get label => switch (this) {
        CompanionVisualModel.classic => 'Classic',
        CompanionVisualModel.stickFigure => 'Stick figure',
      CompanionVisualModel.robot => 'Robot',
      };

  String get command => switch (this) {
        CompanionVisualModel.classic => 'classic',
        CompanionVisualModel.stickFigure => 'stick_figure',
      CompanionVisualModel.robot => 'robot',
      };

  String get description => switch (this) {
        CompanionVisualModel.classic => 'Firmware-faithful face with the full accessory editor.',
        CompanionVisualModel.stickFigure => 'App-first sketch model for playful gestures and scene work.',
      CompanionVisualModel.robot => 'App-first robot duo with panel eyes, antenna beats, and clean geometric poses.',
      };

  bool get supportsClassicAccessories => switch (this) {
        CompanionVisualModel.classic => true,
        CompanionVisualModel.stickFigure => false,
      CompanionVisualModel.robot => false,
      };

  bool get isDeviceSupported => switch (this) {
        CompanionVisualModel.classic => true,
        CompanionVisualModel.stickFigure => false,
      CompanionVisualModel.robot => false,
      };
}

CompanionVisualModel companionVisualModelFromCommand(String? command) {
  final normalized = command?.trim().toLowerCase();
  return CompanionVisualModel.values.firstWhere(
    (model) => model.command == normalized,
    orElse: () => CompanionVisualModel.classic,
  );
}
